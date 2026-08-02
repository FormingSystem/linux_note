#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "protocol/framing.h"
#include "protocol/messages.h"
#include "support/portable_crypto.h"

namespace {

int failures = 0;

void expect(const bool condition, const std::string& message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

void frame_round_trip() {
  std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
  expect(loop::protocol::write_frame(stream, R"({"hello":"world"})"), "frame write succeeds");
  stream.seekg(0);
  const auto result = loop::protocol::read_frame(stream);
  expect(result.status == loop::protocol::read_status::frame, "frame read succeeds");
  expect(result.payload == R"({"hello":"world"})", "frame payload is unchanged");
}

void oversized_frame_is_rejected() {
  std::string bytes(loop::protocol::k_max_control_frame_bytes + 1U, 'x');
  bytes.push_back('\n');
  std::stringstream stream(bytes, std::ios::in | std::ios::binary);
  expect(loop::protocol::read_frame(stream).status == loop::protocol::read_status::error,
      "oversized frame is rejected before allocation");
}

void text_frame_boundaries_are_strict() {
  std::stringstream windows_stream("{\"ok\":true}\r\n", std::ios::in | std::ios::binary);
  const auto windows_result = loop::protocol::read_frame(windows_stream);
  expect(windows_result.status == loop::protocol::read_status::frame, "CRLF frame boundary is accepted");
  expect(windows_result.payload == "{\"ok\":true}", "CR is not part of the JSON payload");

  std::stringstream output;
  expect(!loop::protocol::write_frame(output, "{\n}"), "literal newline inside a control frame is rejected");
}

void valid_handshake_succeeds() {
  loop::protocol::request_handler handler;
  const auto response = nlohmann::json::parse(handler.handle_request(R"({
    "protocol_version": 2,
    "request_id": "invalid",
    "method": "system.handshake",
    "params": {"client_name": "loop_desktop", "client_version": "0.1.0"}
  })"));
  expect(response.at("ok").get<bool>(), "valid handshake succeeds");
  expect(response.at("result").at("language") == "C++", "handshake reports C++ service");
}

void unknown_fields_are_rejected() {
  loop::protocol::request_handler handler;
  const auto response = nlohmann::json::parse(handler.handle_request(R"({
    "protocol_version": 2,
    "request_id": "test-request",
    "method": "system.handshake",
    "params": {"client_name": "loop_desktop", "client_version": "0.1.0"},
    "path": "C:/not-allowed"
  })"));
  expect(!response.at("ok").get<bool>(), "unknown envelope field is rejected");
  expect(response.at("error").at("code") == "INVALID_ENVELOPE", "rejection has stable code");
}

void malformed_json_is_rejected_without_throwing() {
  loop::protocol::request_handler handler;
  const auto response = nlohmann::json::parse(handler.handle_request(R"({"protocol_version":)"));
  expect(!response.at("ok").get<bool>(), "malformed JSON is rejected");
  expect(response.at("error").at("code") == "INVALID_JSON", "malformed JSON has stable code");
}

void protocol_mismatch_is_rejected() {
  loop::protocol::request_handler handler;
  const auto response = nlohmann::json::parse(handler.handle_request(R"({
    "protocol_version": 1,
    "request_id": "version-mismatch",
    "method": "system.handshake",
    "params": {"client_name": "loop_desktop", "client_version": "0.1.0"}
  })"));
  expect(!response.at("ok").get<bool>(), "protocol mismatch is rejected");
  expect(response.at("error").at("code") == "PROTOCOL_MISMATCH", "protocol mismatch has stable code");
}

void unknown_method_is_rejected() {
  loop::protocol::request_handler handler;
  const auto response = nlohmann::json::parse(handler.handle_request(R"({
    "protocol_version": 2,
    "request_id": "unknown-method",
    "method": "system.execute",
    "params": {"client_name": "loop_desktop", "client_version": "0.1.0"}
  })"));
  expect(!response.at("ok").get<bool>(), "unknown method is rejected");
  expect(response.at("error").at("code") == "UNKNOWN_METHOD", "unknown method has stable code");
}

void workspace_methods_keep_paths_inside_native_results() {
  namespace fs = std::filesystem;
  const auto temporary = fs::temp_directory_path()
      / ("loop-protocol-test-" + loop::support::secure_random_hex(8U));
  fs::create_directories(temporary);
  const auto path_value = temporary.u8string();
  const std::string locator(reinterpret_cast<const char*>(path_value.data()), path_value.size());

  loop::protocol::request_handler handler;
  const auto open_request = nlohmann::json{
      {"protocol_version", loop::protocol::k_protocol_version},
      {"request_id", "open-folder"},
      {"method", "workspace.open_folder"},
      {"params", {{"window_session_id", "window_a"}, {"locator", locator}}},
  };
  const auto opened = nlohmann::json::parse(handler.handle_request(open_request.dump()));
  expect(opened.at("ok").get<bool>(), "workspace.open_folder succeeds through protocol");
  expect(opened.dump().find(locator) == std::string::npos, "native result does not expose locator");

  const auto list_request = nlohmann::json{
      {"protocol_version", loop::protocol::k_protocol_version},
      {"request_id", "list-folder"},
      {"method", "workspace.list_children"},
      {"params", {
          {"window_session_id", "window_a"},
          {"workspace_id", opened.at("result").at("workspace_id")},
          {"directory_id", opened.at("result").at("root_directory_id")},
      }},
  };
  const auto listed = nlohmann::json::parse(handler.handle_request(list_request.dump()));
  expect(listed.at("ok").get<bool>(), "workspace.list_children succeeds through protocol");

  auto invalid_request = list_request;
  invalid_request["request_id"] = "invalid-list";
  invalid_request["params"]["path"] = "../escape";
  const auto rejected = nlohmann::json::parse(handler.handle_request(invalid_request.dump()));
  expect(!rejected.at("ok").get<bool>(), "directory path parameter is rejected");
  expect(rejected.at("error").at("code") == "INVALID_PARAMS", "invalid directory request has stable code");

  std::error_code ignored;
  fs::remove_all(temporary, ignored);
}

}  // namespace

int main() {
  frame_round_trip();
  oversized_frame_is_rejected();
  text_frame_boundaries_are_strict();
  valid_handshake_succeeds();
  unknown_fields_are_rejected();
  malformed_json_is_rejected_without_throwing();
  protocol_mismatch_is_rejected();
  unknown_method_is_rejected();
  workspace_methods_keep_paths_inside_native_results();
  if (failures == 0) {
    std::cout << "all native protocol tests passed\n";
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
