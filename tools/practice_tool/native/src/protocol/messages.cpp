#include "protocol/messages.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "support/portable_crypto.h"

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

bool is_sha256_hex(const json& value) {
  if (!value.is_string()) return false;
  const auto& hash = value.get_ref<const std::string&>();
  return hash.size() == 64U
      && std::ranges::all_of(hash, [](const unsigned char character) {
        return (character >= static_cast<unsigned char>('0')
                && character <= static_cast<unsigned char>('9'))
            || (character >= static_cast<unsigned char>('a')
                && character <= static_cast<unsigned char>('f'));
      });
}

service_error protocol_error(std::string code, std::string message) {
  return {std::move(code), std::move(message), false, {}};
}

transport_frame make_control_frame(json control) {
  transport_frame frame;
  frame.control = control.dump();
  return frame;
}

transport_frame error_response(const std::string& request_id, const service_error& error) {
  return make_control_frame(json{
      {"protocol_version", k_protocol_version},
      {"request_id", request_id},
      {"ok", false},
      {"body", nullptr},
      {"error", {
          {"code", error.code},
          {"user_message", error.user_message},
          {"retryable", error.retryable},
          {"recovery_actions", error.recovery_actions},
          {"correlation_id", request_id},
      }},
  });
}

transport_frame success_response(
    const std::string& request_id,
    json result,
    std::vector<unsigned char> body = {},
    const bool body_present = false) {
  json descriptor = nullptr;
  if (body_present) {
    descriptor = json{
        {"kind", "markdown_utf8"},
        {"byte_length", body.size()},
        {"sha256", loop::support::sha256_hex(body)},
    };
  }
  transport_frame frame;
  frame.control = json{
      {"protocol_version", k_protocol_version},
      {"request_id", request_id},
      {"ok", true},
      {"body", std::move(descriptor)},
      {"result", std::move(result)},
  }.dump();
  frame.body = std::move(body);
  frame.body_present = body_present;
  return frame;
}

transport_frame service_response(const std::string& request_id, service_result result) {
  return result.ok
      ? success_response(
          request_id,
          std::move(result.value),
          std::move(result.body),
          result.body_present)
      : error_response(request_id, result.error);
}

bool valid_body_descriptor(
    const json& descriptor,
    const transport_frame& frame,
    const std::string_view expected_kind,
    const bool body_required) {
  if (descriptor.is_null()) {
    return !body_required && !frame.body_present && frame.body.empty();
  }
  if (!frame.body_present || !has_exact_keys(descriptor, {"kind", "byte_length", "sha256"})
      || !body_required
      || descriptor.at("kind") != expected_kind
      || !descriptor.at("byte_length").is_number_unsigned()
      || descriptor.at("byte_length").get<std::uint64_t>() != frame.body.size()
      || !descriptor.at("sha256").is_string()) {
    return false;
  }
  return is_sha256_hex(descriptor.at("sha256"))
      && descriptor.at("sha256").get_ref<const std::string&>()
          == loop::support::sha256_hex(frame.body);
}

}  // namespace

transport_frame request_handler::handle_request(const transport_frame& frame) {
  std::string request_id = "invalid";
  bool request_id_valid = false;
  try {
    const json request = json::parse(frame.control, nullptr, false, false);
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

    if (!has_exact_keys(request, {"protocol_version", "request_id", "method", "params", "body"})) {
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
    const auto save_request = method == "workspace.save_document";
    if (!valid_body_descriptor(
            request.at("body"),
            frame,
            save_request ? "markdown_source_utf8" : "",
            save_request)) {
      return error_response(request_id, protocol_error("INVALID_BODY", "请求正文附件无效或方法不接受附件"));
    }
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
          {"service_version", "0.4.0"},
          {"language", "C++"},
          {"max_control_frame_bytes", k_max_control_frame_bytes},
          {"max_body_frame_bytes", k_max_body_frame_bytes},
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

    if (method == "workspace.open_document") {
      if (!has_exact_keys(
              params,
              {"window_session_id", "workspace_id", "target_kind", "target_id"})
          || !params.at("window_session_id").is_string()
          || !params.at("workspace_id").is_string()
          || !params.at("target_kind").is_string()
          || !params.at("target_id").is_string()
          || !is_bounded_ascii(params.at("window_session_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("workspace_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("target_id").get<std::string>(), 128U)
          || (params.at("target_kind") != "document" && params.at("target_kind") != "entry")) {
        return error_response(request_id, protocol_error("INVALID_PARAMS", "文档打开参数无效"));
      }
      return service_response(
          request_id,
          workspace_service_.open_document(
              params.at("window_session_id").get_ref<const std::string&>(),
              params.at("workspace_id").get_ref<const std::string&>(),
              params.at("target_kind").get_ref<const std::string&>(),
              params.at("target_id").get_ref<const std::string&>()));
    }

    if (method == "workspace.close_document") {
      if (!has_exact_keys(params, {"window_session_id", "workspace_id", "document_id"})
          || !params.at("window_session_id").is_string()
          || !params.at("workspace_id").is_string()
          || !params.at("document_id").is_string()
          || !is_bounded_ascii(params.at("window_session_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("workspace_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("document_id").get<std::string>(), 128U)) {
        return error_response(request_id, protocol_error("INVALID_PARAMS", "文档关闭参数无效"));
      }
      return service_response(
          request_id,
          workspace_service_.close_document(
              params.at("window_session_id").get_ref<const std::string&>(),
              params.at("workspace_id").get_ref<const std::string&>(),
              params.at("document_id").get_ref<const std::string&>()));
    }

    if (method == "workspace.save_document") {
      if (!has_exact_keys(
              params,
              {"window_session_id", "workspace_id", "document_id",
               "expected_file_version_token", "expected_content_hash", "editor_revision",
               "line_ending_policy"})
          || !params.at("window_session_id").is_string()
          || !params.at("workspace_id").is_string()
          || !params.at("document_id").is_string()
          || !params.at("expected_file_version_token").is_string()
          || !params.at("expected_content_hash").is_string()
          || !params.at("editor_revision").is_number_unsigned()
          || !params.at("line_ending_policy").is_string()
          || !is_bounded_ascii(params.at("window_session_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("workspace_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("document_id").get<std::string>(), 128U)
          || !is_bounded_ascii(params.at("expected_file_version_token").get<std::string>(), 128U)
          || !is_sha256_hex(params.at("expected_content_hash"))
          || params.at("editor_revision").get<std::uint64_t>() > 9'007'199'254'740'991ULL
          || (params.at("line_ending_policy") != "preserve"
              && params.at("line_ending_policy") != "normalize_lf"
              && params.at("line_ending_policy") != "normalize_crlf")) {
        return error_response(request_id, protocol_error("INVALID_PARAMS", "文档保存参数无效"));
      }
      return service_response(
          request_id,
          workspace_service_.save_document(
              params.at("window_session_id").get_ref<const std::string&>(),
              params.at("workspace_id").get_ref<const std::string&>(),
              params.at("document_id").get_ref<const std::string&>(),
              params.at("expected_file_version_token").get_ref<const std::string&>(),
              params.at("expected_content_hash").get_ref<const std::string&>(),
              params.at("editor_revision").get<std::uint64_t>(),
              params.at("line_ending_policy").get_ref<const std::string&>(),
              frame.body));
    }

    return error_response(request_id, protocol_error("UNKNOWN_METHOD", "方法不在允许列表中"));
  } catch (...) {
    return error_response(request_id, protocol_error("INTERNAL_ERROR", "本地服务无法完成请求"));
  }
}

}  // namespace loop::protocol
