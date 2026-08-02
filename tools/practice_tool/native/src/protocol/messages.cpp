#include "protocol/messages.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace loop::protocol {
namespace {

using json = nlohmann::json;
using loop::service::service_error;
using loop::service::service_result;

bool has_exact_keys(const json& value, const std::initializer_list<std::string_view> keys) {
  if (!value.is_object() || value.size() != keys.size()) return false;
  return std::ranges::all_of(keys, [&value](const std::string_view key) {
    return value.contains(std::string(key));
  });
}

bool has_required_and_optional_keys(
    const json& value,
    const std::initializer_list<std::string_view> required,
    const std::initializer_list<std::string_view> optional) {
  if (!value.is_object()) return false;
  for (const auto key : required) {
    if (!value.contains(std::string(key))) return false;
  }
  for (const auto& [key, unused] : value.items()) {
    static_cast<void>(unused);
    const auto allowed = std::ranges::any_of(required, [&key](const std::string_view candidate) {
      return key == candidate;
    }) || std::ranges::any_of(optional, [&key](const std::string_view candidate) {
      return key == candidate;
    });
    if (!allowed) return false;
  }
  return true;
}

bool is_bounded_ascii(const std::string& value, const std::size_t maximum_length) {
  return !value.empty() && value.size() <= maximum_length
      && std::ranges::all_of(value, [](const unsigned char character) {
        return character >= 0x20U && character <= 0x7EU;
      });
}

bool is_bounded_text(const json& value, const std::size_t maximum_length) {
  if (!value.is_string()) return false;
  const auto& text = value.get_ref<const std::string&>();
  return !text.empty() && text.size() <= maximum_length && text.find('\0') == std::string::npos;
}

service_error protocol_error(std::string code, std::string message) {
  return {std::move(code), std::move(message), false, {}};
}

std::string error_response(
    const std::string& request_id,
    const service_error& error) {
  return json{
      {"protocol_version", k_protocol_version},
      {"request_id", request_id},
      {"ok", false},
      {"error", {
          {"code", error.code},
          {"user_message", error.user_message},
          {"retryable", error.retryable},
          {"recovery_actions", error.recovery_actions},
          {"correlation_id", request_id},
      }},
  }.dump();
}

std::string success_response(const std::string& request_id, json result) {
  return json{
      {"protocol_version", k_protocol_version},
      {"request_id", request_id},
      {"ok", true},
      {"result", std::move(result)},
  }.dump();
}

std::string service_response(const std::string& request_id, service_result result) {
  return result.ok
      ? success_response(request_id, std::move(result.value))
      : error_response(request_id, result.error);
}

}  // namespace

std::string request_handler::handle_request(const std::string_view payload) {
  std::string request_id = "invalid";
  bool request_id_valid = false;
  try {
    const json request = json::parse(payload, nullptr, false, true);
    if (request.is_discarded() || !request.is_object()) {
      return error_response(request_id, protocol_error("INVALID_JSON", "请求不是有效 JSON 对象"));
    }

    const auto request_id_iterator = request.find("request_id");
    if (request_id_iterator != request.end() && request_id_iterator->is_string()) {
      const auto candidate = request_id_iterator->get<std::string>();
      if (is_bounded_ascii(candidate, 128U)) {
        request_id = candidate;
        request_id_valid = true;
      }
    }

    if (!has_exact_keys(request, {"protocol_version", "request_id", "method", "params"})) {
      return error_response(request_id, protocol_error("INVALID_ENVELOPE", "请求字段不完整或包含未知字段"));
    }
    if (!request.at("protocol_version").is_number_integer()
        || request.at("protocol_version").get<std::int64_t>() != k_protocol_version) {
      return error_response(request_id, protocol_error("PROTOCOL_MISMATCH", "协议版本不匹配"));
    }
    if (!request_id_valid) {
      return error_response(request_id, protocol_error("INVALID_REQUEST_ID", "请求 ID 无效"));
    }
    if (!request.at("method").is_string() || !request.at("params").is_object()) {
      return error_response(request_id, protocol_error("INVALID_METHOD", "方法名或参数无效"));
    }

    const auto method = request.at("method").get<std::string>();
    const auto& params = request.at("params");
    if (method == "system.handshake") {
      if (!has_exact_keys(params, {"client_name", "client_version"})
          || !params.at("client_name").is_string()
          || params.at("client_name").get<std::string>() != "loop_desktop"
          || !params.at("client_version").is_string()
          || !is_bounded_ascii(params.at("client_version").get<std::string>(), 64U)) {
        return error_response(request_id, protocol_error("INVALID_PARAMS", "握手参数无效"));
      }
      return success_response(request_id, json{
          {"service_name", "loop_native_service"},
          {"service_version", "0.2.0"},
          {"language", "C++"},
      });
    }

    if (method == "workspace.open_file" || method == "workspace.open_folder") {
      if (!has_exact_keys(params, {"window_session_id", "locator"})
          || !params.at("window_session_id").is_string()
          || !is_bounded_ascii(params.at("window_session_id").get<std::string>(), 128U)
          || !is_bounded_text(params.at("locator"), 65'536U)) {
        return error_response(request_id, protocol_error("INVALID_PARAMS", "打开参数无效"));
      }
      const auto& window_session_id = params.at("window_session_id").get_ref<const std::string&>();
      const auto& locator = params.at("locator").get_ref<const std::string&>();
      return service_response(
          request_id,
          method == "workspace.open_file"
              ? workspace_service_.open_file(window_session_id, locator)
              : workspace_service_.open_folder(window_session_id, locator));
    }

    if (method == "workspace.close") {
      if (!has_exact_keys(params, {"window_session_id"})
          || !params.at("window_session_id").is_string()
          || !is_bounded_ascii(params.at("window_session_id").get<std::string>(), 128U)) {
        return error_response(request_id, protocol_error("INVALID_PARAMS", "关闭参数无效"));
      }
      return service_response(
          request_id,
          workspace_service_.close(params.at("window_session_id").get_ref<const std::string&>()));
    }

    if (method == "workspace.list_children") {
      if (!has_required_and_optional_keys(
              params,
              {"window_session_id", "workspace_id", "directory_id"},
              {"cursor"})
          || !params.at("window_session_id").is_string()
          || !params.at("workspace_id").is_string()
          || !params.at("directory_id").is_string()
          || !is_bounded_ascii(params.at("window_session_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("workspace_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("directory_id").get<std::string>(), 128U)
          || (params.contains("cursor")
              && (!params.at("cursor").is_string()
                  || !is_bounded_ascii(params.at("cursor").get<std::string>(), 128U)))) {
        return error_response(request_id, protocol_error("INVALID_PARAMS", "目录枚举参数无效"));
      }
      const auto cursor = params.contains("cursor")
          ? params.at("cursor").get<std::string>()
          : std::string();
      return service_response(
          request_id,
          workspace_service_.list_children(
              params.at("window_session_id").get_ref<const std::string&>(),
              params.at("workspace_id").get_ref<const std::string&>(),
              params.at("directory_id").get_ref<const std::string&>(),
              cursor));
    }

    return error_response(request_id, protocol_error("UNKNOWN_METHOD", "方法不在允许列表中"));
  } catch (...) {
    return error_response(request_id, protocol_error("INTERNAL_ERROR", "本地服务无法完成请求"));
  }
}

}  // namespace loop::protocol
