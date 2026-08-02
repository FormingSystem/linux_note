#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "protocol/framing.h"
#include "protocol/messages.h"

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
  expect(result.status == loop::protocol::ReadStatus::frame, "frame read succeeds");
  expect(result.payload == R"({"hello":"world"})", "frame payload is unchanged");
}

void oversized_frame_is_rejected() {
  const auto oversized = static_cast<std::uint32_t>(loop::protocol::kMaxControlFrameBytes + 1U);
  const std::array<unsigned char, 4> header{
      static_cast<unsigned char>((oversized >> 24U) & 0xFFU),
      static_cast<unsigned char>((oversized >> 16U) & 0xFFU),
      static_cast<unsigned char>((oversized >> 8U) & 0xFFU),
      static_cast<unsigned char>(oversized & 0xFFU),
  };
  std::string bytes(reinterpret_cast<const char*>(header.data()), header.size());
  std::stringstream stream(bytes, std::ios::in | std::ios::binary);
  expect(loop::protocol::read_frame(stream).status == loop::protocol::ReadStatus::error,
      "oversized frame is rejected before allocation");
}

void valid_handshake_succeeds() {
  const auto response = nlohmann::json::parse(loop::protocol::handle_request(R"({
    "protocol_version": 1,
    "request_id": "8b19db54-7604-4a29-985a-13806e62b419",
    "method": "system.handshake",
    "params": {"client_name": "loop-desktop", "client_version": "0.1.0"}
  })"));
  expect(response.at("ok").get<bool>(), "valid handshake succeeds");
  expect(response.at("result").at("language") == "C++", "handshake reports C++ service");
}

void unknown_fields_are_rejected() {
  const auto response = nlohmann::json::parse(loop::protocol::handle_request(R"({
    "protocol_version": 1,
    "request_id": "test-request",
    "method": "system.handshake",
    "params": {"client_name": "loop-desktop", "client_version": "0.1.0"},
    "path": "C:/not-allowed"
  })"));
  expect(!response.at("ok").get<bool>(), "unknown envelope field is rejected");
  expect(response.at("error").at("code") == "INVALID_ENVELOPE", "rejection has stable code");
}

void malformed_json_is_rejected_without_throwing() {
  const auto response = nlohmann::json::parse(loop::protocol::handle_request(R"({"protocol_version":)"));
  expect(!response.at("ok").get<bool>(), "malformed JSON is rejected");
  expect(response.at("error").at("code") == "INVALID_JSON", "malformed JSON has stable code");
}

void protocol_mismatch_is_rejected() {
  const auto response = nlohmann::json::parse(loop::protocol::handle_request(R"({
    "protocol_version": 2,
    "request_id": "version-mismatch",
    "method": "system.handshake",
    "params": {"client_name": "loop-desktop", "client_version": "0.1.0"}
  })"));
  expect(!response.at("ok").get<bool>(), "protocol mismatch is rejected");
  expect(response.at("error").at("code") == "PROTOCOL_MISMATCH", "protocol mismatch has stable code");
}

void unknown_method_is_rejected() {
  const auto response = nlohmann::json::parse(loop::protocol::handle_request(R"({
    "protocol_version": 1,
    "request_id": "unknown-method",
    "method": "system.execute",
    "params": {"client_name": "loop-desktop", "client_version": "0.1.0"}
  })"));
  expect(!response.at("ok").get<bool>(), "unknown method is rejected");
  expect(response.at("error").at("code") == "UNKNOWN_METHOD", "unknown method has stable code");
}

}  // namespace

int main() {
  frame_round_trip();
  oversized_frame_is_rejected();
  valid_handshake_succeeds();
  unknown_fields_are_rejected();
  malformed_json_is_rejected_without_throwing();
  protocol_mismatch_is_rejected();
  unknown_method_is_rejected();
  if (failures == 0) {
    std::cout << "all native protocol tests passed\n";
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
