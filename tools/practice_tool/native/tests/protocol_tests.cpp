#include <cstdlib>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "protocol/framing.h"
#include "protocol/messages.h"
#include "support/portable_crypto.h"

namespace {

using json = nlohmann::json;
using loop::protocol::transport_frame;

int failures = 0;

void expect(const bool condition, const std::string& message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

transport_frame control_request(json value) {
  value["body"] = nullptr;
  return {value.dump(), {}, false};
}

json response_control(const transport_frame& frame) {
  return json::parse(frame.control);
}

json handshake_request(const int protocol_version = loop::protocol::k_protocol_version) {
  return {
      {"protocol_version", protocol_version},
      {"request_id", "handshake"},
      {"method", "system.handshake"},
      {"params", {{"client_name", "loop_desktop"}, {"client_version", "0.1.0"}}},
  };
}

void frame_round_trip_and_chunking() {
  const transport_frame source{"{\"hello\":\"world\"}", {'b', 'o', 'd', 'y'}, true};
  const auto encoded = loop::protocol::encode_frame(source);
  loop::protocol::frame_decoder decoder;
  std::vector<transport_frame> decoded;
  for (const auto byte : encoded) {
    const std::array<unsigned char, 1U> chunk{byte};
    auto frames = decoder.push(std::span<const unsigned char>(chunk));
    decoded.insert(decoded.end(), frames.begin(), frames.end());
  }
  expect(decoded.size() == 1U, "arbitrary chunking emits one frame");
  if (!decoded.empty()) {
    expect(decoded.front().control == source.control, "control bytes round trip");
    expect(decoded.front().body == source.body, "body bytes round trip");
    expect(decoded.front().body_present, "body-present flag round trips");
  }
  expect(decoder.finish(), "complete decoder has no trailing bytes");
}

void empty_body_and_multiple_frames_round_trip() {
  const auto first = loop::protocol::encode_frame({"{\"index\":1}", {}, true});
  const auto second = loop::protocol::encode_frame({"{\"index\":2}", {}, false});
  auto combined = first;
  combined.insert(combined.end(), second.begin(), second.end());
  loop::protocol::frame_decoder decoder;
  const auto frames = decoder.push(combined);
  expect(frames.size() == 2U, "multiple frames in one chunk are decoded");
  expect(frames.size() >= 1U && frames[0].body_present && frames[0].body.empty(),
      "empty body remains an explicitly present attachment");
}

void frame_limits_and_headers_fail_closed() {
  transport_frame boundary;
  boundary.control.assign(loop::protocol::k_max_control_frame_bytes, 'x');
  boundary.body.assign(loop::protocol::k_max_body_frame_bytes, 0x61U);
  boundary.body_present = true;
  const auto encoded = loop::protocol::encode_frame(boundary);
  expect(encoded.size() == loop::protocol::k_frame_header_bytes
      + loop::protocol::k_max_control_frame_bytes
      + loop::protocol::k_max_body_frame_bytes, "exact framing limits are accepted");

  bool oversized_rejected = false;
  try {
    boundary.body.push_back(0x61U);
    static_cast<void>(loop::protocol::encode_frame(boundary));
  } catch (const std::runtime_error&) {
    oversized_rejected = true;
  }
  expect(oversized_rejected, "body over limit is rejected");

  for (const auto& mutation : std::vector<std::pair<std::size_t, unsigned char>>{
           {0U, 'X'}, {4U, 2U}, {5U, 2U}, {6U, 1U}, {7U, 1U}}) {
    auto invalid = loop::protocol::encode_frame({"{}", {}, false});
    invalid[mutation.first] = mutation.second;
    bool rejected = false;
    try {
      loop::protocol::frame_decoder decoder;
      static_cast<void>(decoder.push(invalid));
    } catch (const std::runtime_error&) {
      rejected = true;
    }
    expect(rejected, "invalid frame header is rejected");
  }

  loop::protocol::frame_decoder truncated;
  auto incomplete = loop::protocol::encode_frame({"{}", {}, false});
  incomplete.pop_back();
  static_cast<void>(truncated.push(incomplete));
  expect(!truncated.finish(), "truncated frame is detected at EOF");
}

void valid_handshake_succeeds() {
  loop::protocol::request_handler handler;
  const auto response = response_control(handler.handle_request(control_request(handshake_request())));
  expect(response.at("ok").get<bool>(), "valid handshake succeeds");
  expect(response.at("body").is_null(), "handshake has no body");
  expect(response.at("result").at("service_version") == "0.4.0", "handshake reports v0.4 service");
  expect(response.at("result").at("max_body_frame_bytes") == loop::protocol::k_max_body_frame_bytes,
      "handshake reports body limit");
}

void invalid_envelopes_and_bodies_are_rejected() {
  loop::protocol::request_handler handler;
  const auto commented = response_control(handler.handle_request({
      R"({"protocol_version":4,"request_id":"commented","method":"system.handshake","params":{/*not-json*/"client_name":"loop_desktop","client_version":"0.1.0"},"body":null})",
      {},
      false,
  }));
  expect(commented.at("error").at("code") == "INVALID_JSON", "JSON comments are rejected");

  auto unknown = handshake_request();
  unknown["path"] = "C:/not-allowed";
  const auto unknown_response = response_control(handler.handle_request(control_request(unknown)));
  expect(unknown_response.at("error").at("code") == "INVALID_ENVELOPE", "unknown field is rejected");

  const auto malformed = response_control(handler.handle_request({"{\"protocol_version\":", {}, false}));
  expect(malformed.at("error").at("code") == "INVALID_JSON", "malformed JSON is rejected");

  const auto mismatch = response_control(handler.handle_request(control_request(handshake_request(3))));
  expect(mismatch.at("error").at("code") == "PROTOCOL_MISMATCH", "v3 is not accepted by v4");

  auto body_request = handshake_request();
  body_request["body"] = {
      {"kind", "markdown_utf8"},
      {"byte_length", 1U},
      {"sha256", std::string(64U, '0')},
  };
  const auto invalid_body = response_control(handler.handle_request({body_request.dump(), {'x'}, true}));
  expect(invalid_body.at("error").at("code") == "INVALID_BODY", "body digest mismatch is rejected");
}

void workspace_methods_and_document_attachment() {
  namespace fs = std::filesystem;
  const auto temporary = fs::temp_directory_path()
      / ("loop-protocol-test-" + loop::support::secure_random_hex(8U));
  fs::create_directories(temporary);
  const auto markdown = temporary / "README.md";
  {
    std::ofstream output(markdown, std::ios::binary);
    output << "hello\n";
  }
  const auto path_value = markdown.u8string();
  const std::string locator(reinterpret_cast<const char*>(path_value.data()), path_value.size());

  loop::protocol::request_handler handler;
  const auto opened_frame = handler.handle_request(control_request({
      {"protocol_version", loop::protocol::k_protocol_version},
      {"request_id", "open-file"},
      {"method", "workspace.open_file"},
      {"params", {{"window_session_id", "window_a"}, {"locator", locator}}},
  }));
  const auto opened = response_control(opened_frame);
  expect(opened.at("ok").get<bool>(), "workspace.open_file succeeds through protocol");
  expect(opened.dump().find(locator) == std::string::npos, "native result does not expose locator");

  const auto document_frame = handler.handle_request(control_request({
      {"protocol_version", loop::protocol::k_protocol_version},
      {"request_id", "open-document"},
      {"method", "workspace.open_document"},
      {"params", {
          {"window_session_id", "window_a"},
          {"workspace_id", opened.at("result").at("workspace_id")},
          {"target_kind", "document"},
          {"target_id", opened.at("result").at("document").at("document_id")},
      }},
  }));
  const auto document = response_control(document_frame);
  expect(document.at("ok").get<bool>(), "workspace.open_document succeeds through protocol");
  expect(document_frame.body_present, "document response contains attachment");
  expect(std::string(document_frame.body.begin(), document_frame.body.end()) == "hello\n",
      "document attachment contains markdown bytes");
  expect(document.at("body").at("byte_length") == document_frame.body.size(),
      "document descriptor length matches attachment");
  expect(document.at("body").at("sha256") == loop::support::sha256_hex(document_frame.body),
      "document descriptor digest matches attachment");

  const std::vector<unsigned char> saved_body{'s', 'a', 'v', 'e', 'd', '\n'};
  json save_control{
      {"protocol_version", loop::protocol::k_protocol_version},
      {"request_id", "save-document"},
      {"method", "workspace.save_document"},
      {"params", {
          {"window_session_id", "window_a"},
          {"workspace_id", opened.at("result").at("workspace_id")},
          {"document_id", document.at("result").at("document_id")},
          {"expected_file_version_token", document.at("result").at("file_version_token")},
          {"expected_content_hash", document.at("result").at("content_hash")},
          {"editor_revision", 1U},
          {"line_ending_policy", "preserve"},
      }},
      {"body", {
          {"kind", "markdown_source_utf8"},
          {"byte_length", saved_body.size()},
          {"sha256", loop::support::sha256_hex(saved_body)},
      }},
  };
  const auto save_frame = handler.handle_request({save_control.dump(), saved_body, true});
  const auto saved = response_control(save_frame);
  expect(saved.at("ok").get<bool>() && saved.at("body").is_null() && !save_frame.body_present,
      "workspace.save_document accepts only a request attachment and returns control metadata");
  expect(saved.at("result").at("saved_revision") == 1U,
      "save protocol returns the acknowledged editor revision");

  save_control["request_id"] = "wrong-save-kind";
  save_control["body"]["kind"] = "markdown_utf8";
  const auto wrong_kind = response_control(handler.handle_request({save_control.dump(), saved_body, true}));
  expect(wrong_kind.at("error").at("code") == "INVALID_BODY",
      "save rejects the response-only Markdown attachment kind");

  save_control["request_id"] = "missing-save-body";
  save_control["body"] = nullptr;
  const auto missing_body = response_control(handler.handle_request({save_control.dump(), {}, false}));
  expect(missing_body.at("error").at("code") == "INVALID_BODY",
      "save rejects a missing request attachment");

  save_control["request_id"] = "invalid-expected-hash";
  save_control["body"] = {
      {"kind", "markdown_source_utf8"},
      {"byte_length", saved_body.size()},
      {"sha256", loop::support::sha256_hex(saved_body)},
  };
  save_control["params"]["expected_content_hash"] = std::string(64U, 'G');
  const auto invalid_expected_hash = response_control(
      handler.handle_request({save_control.dump(), saved_body, true}));
  expect(invalid_expected_hash.at("error").at("code") == "INVALID_PARAMS",
      "save rejects a non-lowercase-hex expected digest");

  const auto closed_document = response_control(handler.handle_request(control_request(json{
      {"protocol_version", loop::protocol::k_protocol_version},
      {"request_id", "close-document"},
      {"method", "workspace.close_document"},
      {"params", {
          {"window_session_id", "window_a"},
          {"workspace_id", opened.at("result").at("workspace_id")},
          {"document_id", document.at("result").at("document_id")},
      }},
      {"body", nullptr},
  })));
  expect(closed_document.at("ok").get<bool>() && closed_document.at("result").at("closed") == true,
      "workspace.close_document revokes the document through protocol v4");

  std::error_code ignored;
  fs::remove_all(temporary, ignored);
}

}  // namespace

int main() {
  frame_round_trip_and_chunking();
  empty_body_and_multiple_frames_round_trip();
  frame_limits_and_headers_fail_closed();
  valid_handshake_succeeds();
  invalid_envelopes_and_bodies_are_rejected();
  workspace_methods_and_document_attachment();
  if (failures == 0) {
    std::cout << "all native protocol tests passed\n";
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
