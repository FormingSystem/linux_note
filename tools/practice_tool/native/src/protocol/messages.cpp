#include "protocol/messages.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <string>

#include <nlohmann/json.hpp>

namespace loop::protocol {
namespace {

using Json = nlohmann::json;

bool has_exact_keys(const Json& value, const std::initializer_list<std::string_view> keys) {
  if (!value.is_object() || value.size() != keys.size()) return false;
  return std::ranges::all_of(keys, [&value](const std::string_view key) {
    return value.contains(std::string(key));
  });
}

bool is_bounded_ascii(const std::string& value, const std::size_t maximum_length) {
  return !value.empty() && value.size() <= maximum_length
      && std::ranges::all_of(value, [](const unsigned char character) {
        return character >= 0x20U && character <= 0x7EU;
      });
}

std::string error_response(const std::string& request_id, const std::string& code, const std::string& message) {
  return Json{
      {"protocol_version", kProtocolVersion},
      {"request_id", request_id},
      {"ok", false},
      {"error", {{"code", code}, {"message", message}}},
  }.dump();
}

}  // namespace

std::string handle_request(const std::string_view payload) {
  std::string request_id = "invalid";
  try {
    const Json request = Json::parse(payload, nullptr, false, true);
    if (request.is_discarded() || !request.is_object()) {
      return error_response(request_id, "INVALID_JSON", "请求不是有效 JSON 对象");
    }

    const auto request_id_iterator = request.find("request_id");
    if (request_id_iterator != request.end() && request_id_iterator->is_string()) {
      const auto candidate = request_id_iterator->get<std::string>();
      if (is_bounded_ascii(candidate, 128U)) request_id = candidate;
    }

    if (!has_exact_keys(request, {"protocol_version", "request_id", "method", "params"})) {
      return error_response(request_id, "INVALID_ENVELOPE", "请求字段不完整或包含未知字段");
    }
    if (!request.at("protocol_version").is_number_integer()
        || request.at("protocol_version").get<std::int64_t>() != kProtocolVersion) {
      return error_response(request_id, "PROTOCOL_MISMATCH", "协议版本不匹配");
    }
    if (request_id == "invalid") {
      return error_response(request_id, "INVALID_REQUEST_ID", "请求 ID 无效");
    }
    if (!request.at("method").is_string()) {
      return error_response(request_id, "INVALID_METHOD", "方法名无效");
    }

    const auto method = request.at("method").get<std::string>();
    if (method != "system.handshake") {
      return error_response(request_id, "UNKNOWN_METHOD", "方法不在允许列表中");
    }

    const auto& params = request.at("params");
    if (!has_exact_keys(params, {"client_name", "client_version"})
        || !params.at("client_name").is_string()
        || params.at("client_name").get<std::string>() != "loop-desktop"
        || !params.at("client_version").is_string()
        || !is_bounded_ascii(params.at("client_version").get<std::string>(), 64U)) {
      return error_response(request_id, "INVALID_PARAMS", "握手参数无效");
    }

    return Json{
        {"protocol_version", kProtocolVersion},
        {"request_id", request_id},
        {"ok", true},
        {"result", {
            {"service_name", "loop-native-service"},
            {"service_version", "0.1.0"},
            {"language", "C++"},
        }},
    }.dump();
  } catch (...) {
    return error_response(request_id, "INVALID_REQUEST", "请求无法安全解析");
  }
}

}  // namespace loop::protocol
