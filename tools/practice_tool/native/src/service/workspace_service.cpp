#include "service/workspace_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cwctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "support/portable_crypto.h"
#include "service/filesystem_capability_port.h"

#include <uv.h>

namespace loop::service {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

constexpr std::size_t k_maximum_workspace_entries = 100'000U;
constexpr std::size_t k_maximum_active_cursors = 8U;
constexpr std::size_t k_maximum_page_payload_bytes = 512U * 1024U;

struct path_info {
  fs::path canonical_path;
  std::string identity;
  std::uint64_t device = 0U;
  std::uint64_t size = 0U;
  std::int64_t write_seconds = 0;
  std::int64_t write_nanoseconds = 0;
  bool regular_file = false;
  bool directory = false;
  bool read_only = false;
};

std::string new_id(const std::string_view prefix) {
  return std::string(prefix) + '_' + loop::support::secure_random_hex(16U);
}

std::string lower_ascii(std::string value) {
  std::ranges::transform(value, value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

fs::path path_from_utf8(const std::string_view value) {
  std::u8string converted;
  converted.reserve(value.size());
  for (const char raw_character : value) {
    converted.push_back(static_cast<char8_t>(static_cast<unsigned char>(raw_character)));
  }
  return fs::path(converted);
}

std::string path_bytes(const fs::path& path) {
  const auto converted = path.u8string();
  return {reinterpret_cast<const char*>(converted.data()), converted.size()};
}

bool valid_utf8(const std::span<const unsigned char> bytes) {
  std::size_t index = 0U;
  while (index < bytes.size()) {
    const auto first = bytes[index];
    if (first <= 0x7FU) {
      ++index;
      continue;
    }

    std::size_t length = 0U;
    std::uint32_t code_point = 0U;
    if (first >= 0xC2U && first <= 0xDFU) {
      length = 2U;
      code_point = static_cast<std::uint32_t>(first & 0x1FU);
    } else if (first >= 0xE0U && first <= 0xEFU) {
      length = 3U;
      code_point = static_cast<std::uint32_t>(first & 0x0FU);
    } else if (first >= 0xF0U && first <= 0xF4U) {
      length = 4U;
      code_point = static_cast<std::uint32_t>(first & 0x07U);
    } else {
      return false;
    }
    if (index + length > bytes.size()) return false;
    for (std::size_t offset = 1U; offset < length; ++offset) {
      const auto continuation = bytes[index + offset];
      if ((continuation & 0xC0U) != 0x80U) return false;
      code_point = (code_point << 6U) | static_cast<std::uint32_t>(continuation & 0x3FU);
    }
    if ((length == 3U && code_point < 0x800U)
        || (length == 4U && code_point < 0x10000U)
        || (code_point >= 0xD800U && code_point <= 0xDFFFU)
        || code_point > 0x10FFFFU) {
      return false;
    }
    index += length;
  }
  return true;
}

std::string escaped_display_name(const std::string& bytes) {
  const auto byte_span = std::span(
      reinterpret_cast<const unsigned char*>(bytes.data()),
      bytes.size());
  if (valid_utf8(byte_span) && !bytes.empty() && bytes.size() <= 1024U) return bytes;

  constexpr char hex[] = "0123456789ABCDEF";
  std::string escaped;
  escaped.reserve(std::min<std::size_t>(bytes.size() * 4U, 1024U));
  for (const char raw_byte : bytes) {
    const auto byte = static_cast<unsigned char>(raw_byte);
    if (escaped.size() + 4U > 1024U) break;
    if (byte >= 0x20U && byte <= 0x7EU && byte != '\\') {
      escaped.push_back(static_cast<char>(byte));
    } else {
      escaped.push_back('\\');
      escaped.push_back('x');
      escaped.push_back(hex[(byte >> 4U) & 0x0FU]);
      escaped.push_back(hex[byte & 0x0FU]);
    }
  }
  return escaped.empty() ? std::string("<无法显示的名称>") : escaped;
}

std::string safe_name(const fs::path& path) {
  try {
    auto bytes = path_bytes(path.filename());
    if (bytes.empty()) bytes = path_bytes(path.root_name());
    return escaped_display_name(bytes);
  } catch (...) {
    return "<无法显示的名称>";
  }
}

std::string join_display_path(const std::string& parent, const std::string& name) {
  const auto combined = parent.empty() ? name : parent + '/' + name;
  if (combined.size() <= 4096U) return combined;
  const auto suffix = std::string(".../") + name;
  return suffix.size() <= 4096U ? suffix : std::string("<路径过长>");
}

bool is_markdown_path(const fs::path& path) {
  const auto extension = lower_ascii(path_bytes(path.extension()));
  return extension == ".md" || extension == ".markdown";
}

service_error make_error(
    std::string code,
    std::string message,
    const bool retryable = false,
    std::vector<std::string> recovery_actions = {}) {
  return {std::move(code), std::move(message), retryable, std::move(recovery_actions)};
}

class uv_file_request {
 public:
  uv_file_request() = default;
  ~uv_file_request() { uv_fs_req_cleanup(&request_); }
  uv_file_request(const uv_file_request&) = delete;
  uv_file_request& operator=(const uv_file_request&) = delete;
  [[nodiscard]] uv_fs_t* get() { return &request_; }

 private:
  uv_fs_t request_{};
};

std::string uv_error_code(const int error) {
  if (error == UV_ENOENT || error == UV_ENOTDIR) return "NOT_FOUND";
  if (error == UV_EACCES || error == UV_EPERM) return "PERMISSION_DENIED";
  return "INTERNAL_ERROR";
}

std::optional<std::string> path_identity(const fs::path& path) {
  std::string encoded;
  try {
    encoded = path_bytes(path);
  } catch (...) {
    return std::nullopt;
  }
  uv_file_request request;
  if (uv_fs_stat(nullptr, request.get(), encoded.c_str(), nullptr) < 0) return std::nullopt;
  const auto& stat = request.get()->statbuf;
  return std::to_string(stat.st_dev) + ':' + std::to_string(stat.st_ino);
}

bool path_was_link(const fs::path& path) {
  std::string encoded;
  try {
    encoded = path_bytes(path);
  } catch (...) {
    return false;
  }
  uv_file_request request;
  if (uv_fs_lstat(nullptr, request.get(), encoded.c_str(), nullptr) < 0) return false;
  const auto mode = request.get()->statbuf.st_mode;
  return (mode & static_cast<std::uint64_t>(S_IFMT)) == static_cast<std::uint64_t>(S_IFLNK);
}

std::optional<path_info> query_path(const fs::path& path, std::string& failure_code) {
  std::string encoded;
  try {
    encoded = path_bytes(path);
  } catch (...) {
    failure_code = "INTERNAL_ERROR";
    return std::nullopt;
  }

  uv_file_request realpath_request;
  const auto realpath_result = uv_fs_realpath(
      nullptr, realpath_request.get(), encoded.c_str(), nullptr);
  if (realpath_result < 0 || realpath_request.get()->ptr == nullptr) {
    failure_code = uv_error_code(realpath_result);
    return std::nullopt;
  }
  const auto* realpath_bytes = static_cast<const char*>(realpath_request.get()->ptr);
  fs::path canonical_path;
  try {
    canonical_path = path_from_utf8(realpath_bytes);
  } catch (...) {
    failure_code = "INTERNAL_ERROR";
    return std::nullopt;
  }

  uv_file_request stat_request;
  const auto stat_result = uv_fs_stat(
      nullptr, stat_request.get(), realpath_bytes, nullptr);
  if (stat_result < 0) {
    failure_code = uv_error_code(stat_result);
    return std::nullopt;
  }
  const auto& stat = stat_request.get()->statbuf;
  const auto file_type = stat.st_mode & static_cast<std::uint64_t>(S_IFMT);

  path_info result;
  result.canonical_path = canonical_path;
  result.device = stat.st_dev;
  result.identity = std::to_string(stat.st_dev) + ':' + std::to_string(stat.st_ino);
  result.directory = file_type == static_cast<std::uint64_t>(S_IFDIR);
  result.regular_file = file_type == static_cast<std::uint64_t>(S_IFREG);
  result.read_only = (stat.st_mode & 0222U) == 0U;
  if (result.regular_file) {
    result.size = stat.st_size;
    result.write_seconds = stat.st_mtim.tv_sec;
    result.write_nanoseconds = stat.st_mtim.tv_nsec;
  }
  return result;
}

bool same_path_entry(const fs::path& first, const fs::path& second) {
  const auto first_identity = path_identity(first);
  const auto second_identity = path_identity(second);
  return first_identity && second_identity && *first_identity == *second_identity;
}

bool path_is_within(const fs::path& root, const fs::path& candidate) {
  auto current = candidate;
  while (!current.empty()) {
    if (same_path_entry(root, current)) return true;
    const auto parent = current.parent_path();
    if (parent == current) break;
    current = parent;
  }
  return false;
}

service_result failure_for_path(const std::string& code, const std::string& target_kind) {
  if (code == "NOT_FOUND") {
    return service_result::failure(make_error(
        "NOT_FOUND", target_kind + "不存在或已被移动", true, {"RETRY", "CHOOSE_ANOTHER"}));
  }
  if (code == "PERMISSION_DENIED") {
    return service_result::failure(make_error(
        "PERMISSION_DENIED", "没有读取所选" + target_kind + "的权限", true, {"RETRY", "CHOOSE_ANOTHER"}));
  }
  return service_result::failure(make_error(
      "INTERNAL_ERROR", "无法安全检查所选" + target_kind, false, {"CHOOSE_ANOTHER"}));
}

service_result failure_for_document_read(const std::string& code) {
  if (code == "NOT_FOUND" || code == "PERMISSION_DENIED") return failure_for_path(code, "文件");
  if (code == "NOT_REGULAR_FILE") {
    return service_result::failure(make_error(
        "NOT_REGULAR_FILE", "文档能力不再指向普通文件", false, {"REOPEN_WORKSPACE"}));
  }
  if (code == "CONTENT_TOO_LARGE") {
    return service_result::failure(make_error(
        "CONTENT_TOO_LARGE", "Markdown 文件超过 5 MiB 首版限制", false, {"CHOOSE_ANOTHER"}));
  }
  if (code == "INVALID_ENCODING") {
    return service_result::failure(make_error(
        "INVALID_ENCODING", "文件不是有效的 UTF-8 Markdown，已拒绝替换字符解码", false,
        {"CHOOSE_ANOTHER"}));
  }
  if (code == "WORKSPACE_INVALID") {
    return service_result::failure(make_error(
        "WORKSPACE_INVALID", "文件身份或内容在读取期间发生变化，请重新打开", true,
        {"REOPEN_WORKSPACE"}));
  }
  return service_result::failure(make_error(
      "INTERNAL_ERROR", "无法安全读取 Markdown 正文", false, {"REOPEN_WORKSPACE"}));
}

service_result failure_for_capability(const std::string& code, const std::string& target_kind) {
  if (code == "NOT_FOUND" || code == "PERMISSION_DENIED") {
    return failure_for_path(code, target_kind);
  }
  if (code == "NOT_DIRECTORY") {
    return service_result::failure(make_error(
        "NOT_DIRECTORY", "所选目标不是文件夹", false, {"CHOOSE_ANOTHER"}));
  }
  if (code == "DIRECTORY_RESOURCE_LIMIT") {
    return service_result::failure(make_error(
        "DIRECTORY_RESOURCE_LIMIT", "单个目录超过 50000 项或 32 MiB 元数据限制", false,
        {"REFINE_SCOPE"}));
  }
  if (code == "LINK_OR_MOUNT_BLOCKED") {
    return service_result::failure(make_error(
        "PATH_OUTSIDE_WORKSPACE", "目录解析遇到链接、挂载或工作区边界", false,
        {"REOPEN_WORKSPACE", "CHOOSE_ANOTHER"}));
  }
  if (code == "PLATFORM_UNSUPPORTED" || code == "NETWORK_FILESYSTEM_UNSUPPORTED") {
    return service_result::failure(make_error(
        "INTERNAL_ERROR", "当前平台或文件系统不能提供安全目录能力", false,
        {"CHOOSE_ANOTHER"}));
  }
  return service_result::failure(make_error(
      "WORKSPACE_INVALID", target_kind + "能力已经失效，请重新打开工作区", true,
      {"REOPEN_WORKSPACE"}));
}

service_result failure_for_save(const std::string& code) {
  if (code == "DOCUMENT_CONFLICT" || code == "WORKSPACE_INVALID" || code == "NOT_FOUND") {
    return service_result::failure(make_error(
        "DOCUMENT_CONFLICT", "磁盘文件已经变化，未覆盖外部内容", false,
        {"REOPEN_WORKSPACE"}));
  }
  if (code == "PERMISSION_DENIED") {
    return service_result::failure(make_error(
        "PERMISSION_DENIED", "没有权限安全替换此文件", false, {"CHOOSE_ANOTHER"}));
  }
  if (code == "FILE_BUSY") {
    return service_result::failure(make_error(
        "FILE_BUSY", "文件正被其他程序占用，未执行替换", true, {"RETRY"}));
  }
  if (code == "DISK_FULL") {
    return service_result::failure(make_error(
        "DISK_FULL", "磁盘空间不足，原文件未被替换", true, {"RETRY"}));
  }
  if (code == "UNSAFE_FILE_METADATA") {
    return service_result::failure(make_error(
        "UNSAFE_FILE_METADATA", "此文件的链接或元数据不能由当前安全保存策略保持", false,
        {"CHOOSE_ANOTHER"}));
  }
  if (code == "SAVE_OUTCOME_UNKNOWN") {
    return service_result::failure(make_error(
        "SAVE_OUTCOME_UNKNOWN", "替换结果无法确认，请重新打开并比较磁盘内容", false,
        {"REOPEN_WORKSPACE"}));
  }
  return service_result::failure(make_error(
      "INTERNAL_ERROR", "无法安全保存 Markdown", false, {"REOPEN_WORKSPACE"}));
}

struct markdown_serialization_result {
  bool ok = false;
  std::string error_code;
  std::string line_ending;
  std::vector<unsigned char> bytes;
};

markdown_serialization_result serialize_markdown_for_save(
    const std::span<const unsigned char> content,
    const bool bom,
    const std::string_view baseline_line_ending,
    const std::string_view line_ending_policy) {
  markdown_serialization_result result;
  markdown_inspection inspection;
  if (!inspect_markdown_bytes(content, inspection)
      || std::ranges::find(content, static_cast<unsigned char>('\r')) != content.end()) {
    result.error_code = "INVALID_ENCODING";
    return result;
  }
  if (baseline_line_ending == "mixed" && line_ending_policy == "preserve") {
    result.error_code = "FORMAT_DECISION_REQUIRED";
    return result;
  }
  if (line_ending_policy == "normalize_crlf"
      || (line_ending_policy == "preserve" && baseline_line_ending == "crlf")) {
    result.line_ending = "crlf";
  } else if (line_ending_policy == "normalize_lf"
      || line_ending_policy == "preserve") {
    result.line_ending = "lf";
  } else {
    result.error_code = "INVALID_REQUEST";
    return result;
  }

  const auto line_feed_count = static_cast<std::size_t>(
      std::ranges::count(content, static_cast<unsigned char>('\n')));
  const auto bom_bytes = bom ? 3U : 0U;
  const auto crlf_expansion = result.line_ending == "crlf" ? line_feed_count : 0U;
  if (content.size() > k_maximum_markdown_bytes - bom_bytes
      || crlf_expansion > k_maximum_markdown_bytes - bom_bytes - content.size()) {
    result.error_code = "CONTENT_TOO_LARGE";
    return result;
  }
  result.bytes.reserve(bom_bytes + content.size() + crlf_expansion);
  if (bom) {
    result.bytes.insert(result.bytes.end(), {0xEFU, 0xBBU, 0xBFU});
  }
  for (const auto byte : content) {
    if (byte == static_cast<unsigned char>('\n') && result.line_ending == "crlf") {
      result.bytes.push_back(static_cast<unsigned char>('\r'));
    }
    result.bytes.push_back(byte);
  }
  result.ok = true;
  return result;
}

struct entry_record {
  std::string id;
  std::string parent_id;
  std::string name;
  filesystem_component_chain components;
  std::string identity;
  std::string relative_display;
  bool directory = false;
  bool markdown = false;
  bool accessible = false;
};

struct document_record {
  std::string id;
  std::string name;
  std::string display_path;
  filesystem_component_chain components;
  std::string identity;
  std::string content_hash;
  std::string file_version_token;
  std::string line_ending = "none";
  std::uint64_t link_count = 0U;
  bool bom = false;
  bool read_only = false;
  bool resolved_from_link = false;
};

struct cursor_state {
  std::string directory_id;
  std::vector<json> items;
  std::size_t offset = 0U;
};

struct workspace_capability {
  std::string id;
  std::string window_session_id;
  std::string mode;
  filesystem_workspace_root root;
  std::unordered_map<std::string, entry_record> entries;
  std::unordered_map<std::string, cursor_state> cursors;
  std::unordered_map<std::string, document_record> documents;
  std::unordered_map<std::string, std::string> document_ids_by_identity;
};

}  // namespace

service_result service_result::success(json value) {
  service_result result;
  result.ok = true;
  result.value = std::move(value);
  return result;
}

service_result service_result::success_with_body(json value, std::vector<unsigned char> body) {
  service_result result;
  result.ok = true;
  result.value = std::move(value);
  result.body = std::move(body);
  result.body_present = true;
  return result;
}

service_result service_result::failure(service_error error) {
  service_result result;
  result.error = std::move(error);
  return result;
}

class workspace_service::impl {
 public:
  service_result open_file(const std::string_view window_session_id, const std::string_view locator) {
    fs::path requested;
    try {
      requested = path_from_utf8(locator);
    } catch (...) {
      return service_result::failure(make_error(
          "INVALID_REQUEST", "所选文件路径不是有效的 UTF-8", false, {"CHOOSE_ANOTHER"}));
    }

    std::string failure_code;
    const auto initial = query_path(requested, failure_code);
    if (!initial) return failure_for_path(failure_code, "文件");
    if (!initial->regular_file) {
      return service_result::failure(make_error(
          "NOT_REGULAR_FILE", "所选目标不是普通文件", false, {"CHOOSE_ANOTHER"}));
    }
    if (!is_markdown_path(initial->canonical_path)) {
      return service_result::failure(make_error(
          "NOT_REGULAR_FILE", "当前只支持 .md 和 .markdown 文件", false, {"CHOOSE_ANOTHER"}));
    }
    const auto read = files_.read_markdown(initial->canonical_path, initial->identity);
    if (!read.ok) return failure_for_document_read(read.error_code);
    if (!path_is_within(initial->canonical_path.parent_path(), read.canonical_path)) {
      return service_result::failure(make_error(
          "PATH_OUTSIDE_WORKSPACE", "文件解析结果越过已选择边界", false, {"CHOOSE_ANOTHER"}));
    }

    auto root_open = capabilities_.open_root(read.canonical_path.parent_path());
    if (!root_open.ok) return failure_for_capability(root_open.error_code, "文件父目录");
    std::string selected_component;
    try {
      selected_component = path_bytes(read.canonical_path.filename());
    } catch (...) {
      return service_result::failure(make_error(
          "INVALID_REQUEST", "所选文件名无法安全表示", false, {"CHOOSE_ANOTHER"}));
    }
    if (!is_safe_native_capability_child_name(selected_component)) {
      return service_result::failure(make_error(
          "INVALID_REQUEST", "所选文件名不是安全的单个路径组件", false, {"CHOOSE_ANOTHER"}));
    }
    filesystem_component_chain components{selected_component};
    auto authorized = capabilities_.authorize_regular_file(root_open.root, components);
    if (!authorized.ok) return failure_for_capability(authorized.error_code, "文件");
    const auto capability_identity = authorized.identity;
    const auto capability_read = files_.read_markdown(std::move(authorized.file));
    if (!capability_read.ok) return failure_for_document_read(capability_read.error_code);
    if (capability_read.content_hash != read.content_hash
        || !capabilities_.verify_regular_file(root_open.root, components, capability_identity)) {
      return service_result::failure(make_error(
          "WORKSPACE_INVALID", "所选文件在能力建立期间发生变化，请重新打开", true,
          {"REOPEN_WORKSPACE"}));
    }

    workspace_capability workspace;
    workspace.id = new_id("workspace");
    workspace.window_session_id = std::string(window_session_id);
    workspace.mode = "single_file";
    workspace.root = std::move(root_open.root);

    document_record document;
    document.id = new_id("document");
    document.components = std::move(components);
    document.identity = capability_identity;
    document.content_hash = capability_read.content_hash;
    document.file_version_token = new_id("version");
    document.name = safe_name(read.canonical_path);
    document.display_path = document.name;
    document.bom = capability_read.inspection.bom;
    document.line_ending = capability_read.inspection.line_ending;
    document.link_count = capability_read.link_count;
    document.read_only = capability_read.read_only;
    const auto resolved_from_link = path_was_link(requested);
    document.resolved_from_link = resolved_from_link;
    const auto document_id = document.id;
    const auto display_name = document.name;
    workspace.document_ids_by_identity.emplace(document.identity, document.id);
    workspace.documents.emplace(document.id, std::move(document));
    const auto result = json{
        {"workspace_id", workspace.id},
        {"mode", "single_file"},
        {"display_name", display_name},
        {"document", {
            {"document_id", document_id},
            {"name", display_name},
            {"display_path", display_name},
            {"byte_size", capability_read.byte_size},
            {"encoding", "utf-8"},
            {"bom", capability_read.inspection.bom},
            {"line_ending", capability_read.inspection.line_ending},
            {"read_only", capability_read.read_only},
            {"resolved_from_link", resolved_from_link},
        }},
    };
    workspaces_[std::string(window_session_id)] = std::move(workspace);
    return service_result::success(result);
  }

  service_result open_folder(const std::string_view window_session_id, const std::string_view locator) {
    fs::path requested;
    try {
      requested = path_from_utf8(locator);
    } catch (...) {
      return service_result::failure(make_error(
          "INVALID_REQUEST", "所选文件夹路径不是有效的 UTF-8", false, {"CHOOSE_ANOTHER"}));
    }

    auto root_open = capabilities_.open_root(requested);
    if (!root_open.ok) return failure_for_capability(root_open.error_code, "文件夹");

    workspace_capability workspace;
    workspace.id = new_id("workspace");
    workspace.window_session_id = std::string(window_session_id);
    workspace.mode = "folder";
    workspace.root = std::move(root_open.root);

    entry_record root;
    root.id = new_id("directory");
    root.identity = root_open.identity;
    root.directory = true;
    root.accessible = true;
    const auto root_id = root.id;
    workspace.entries.emplace(root.id, std::move(root));

    const auto display_name = safe_name(requested);
    const auto result = json{
        {"workspace_id", workspace.id},
        {"mode", "folder"},
        {"display_name", display_name},
        {"root_directory_id", root_id},
        {"resolved_from_link", path_was_link(requested)},
    };
    workspaces_[std::string(window_session_id)] = std::move(workspace);
    return service_result::success(result);
  }

  service_result close(const std::string_view window_session_id) {
    workspaces_.erase(std::string(window_session_id));
    return service_result::success(json{{"closed", true}});
  }

  service_result close_document(
      const std::string_view window_session_id,
      const std::string_view workspace_id,
      const std::string_view document_id) {
    const auto workspace_iterator = workspaces_.find(std::string(window_session_id));
    if (workspace_iterator == workspaces_.end() || workspace_iterator->second.id != workspace_id) {
      return invalid_workspace();
    }
    auto& workspace = workspace_iterator->second;
    const auto document_iterator = workspace.documents.find(std::string(document_id));
    if (document_iterator == workspace.documents.end()) return invalid_workspace();
    const auto identity = document_iterator->second.identity;
    if (const auto identity_iterator = workspace.document_ids_by_identity.find(identity);
        identity_iterator != workspace.document_ids_by_identity.end()
            && identity_iterator->second == document_id) {
      workspace.document_ids_by_identity.erase(identity_iterator);
    }
    workspace.documents.erase(document_iterator);
    return service_result::success(json{{"closed", true}});
  }

  service_result list_children(
      const std::string_view window_session_id,
      const std::string_view workspace_id,
      const std::string_view directory_id,
      const std::string_view cursor) {
    auto workspace_iterator = workspaces_.find(std::string(window_session_id));
    if (workspace_iterator == workspaces_.end() || workspace_iterator->second.id != workspace_id
        || workspace_iterator->second.mode != "folder") {
      return invalid_workspace();
    }
    auto& workspace = workspace_iterator->second;

    if (!cursor.empty()) {
      auto cursor_iterator = workspace.cursors.find(std::string(cursor));
      if (cursor_iterator == workspace.cursors.end() || cursor_iterator->second.directory_id != directory_id) {
        return invalid_workspace();
      }
      auto state = std::move(cursor_iterator->second);
      workspace.cursors.erase(cursor_iterator);
      return make_page(workspace, std::move(state.items), state.offset, std::string(directory_id));
    }

    const auto directory_iterator = workspace.entries.find(std::string(directory_id));
    if (directory_iterator == workspace.entries.end() || !directory_iterator->second.directory
        || !directory_iterator->second.accessible) {
      return invalid_workspace();
    }
    const auto& directory = directory_iterator->second;
    const auto enumeration = capabilities_.list_directory(
        workspace.root,
        directory.components,
        directory.identity,
        k_maximum_directory_entries,
        k_maximum_directory_metadata_bytes);
    if (!enumeration.ok) return failure_for_capability(enumeration.error_code, "文件夹");

    std::vector<json> items;
    std::vector<entry_record> records;
    items.reserve(enumeration.entries.size());
    records.reserve(enumeration.entries.size());
    for (const auto& item : enumeration.entries) {
      const auto& name = item.display_name;
      const auto relative_display = join_display_path(directory.relative_display, name);

      entry_record record;
      record.id = new_id("entry");
      record.parent_id = directory.id;
      record.name = name;
      record.relative_display = relative_display;

      std::string kind = "other";
      bool expandable = false;
      bool accessible = false;
      json byte_size = nullptr;
      if (item.kind == filesystem_capability_entry_kind::symbolic_link) {
        kind = "symbolic_link";
      } else if (item.kind == filesystem_capability_entry_kind::directory) {
        kind = "directory";
        if (item.accessible && !item.name.empty() && !item.identity.empty()) {
          expandable = true;
          accessible = true;
          record.components = directory.components;
          record.components.push_back(item.name);
          record.identity = item.identity;
          record.directory = true;
          record.accessible = true;
        }
      } else if (item.kind == filesystem_capability_entry_kind::regular_file) {
        const auto markdown = !item.name.empty() && is_markdown_path(path_from_utf8(item.name));
        kind = markdown ? "markdown" : "file";
        if (item.accessible && !item.name.empty() && !item.identity.empty()) {
          accessible = true;
          record.components = directory.components;
          record.components.push_back(item.name);
          record.identity = item.identity;
          record.accessible = true;
          record.markdown = markdown;
          if (item.byte_size
              && *item.byte_size <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            byte_size = *item.byte_size;
          }
        }
      }

      items.push_back(json{
          {"entry_id", record.id},
          {"parent_id", directory.id},
          {"name", name},
          {"relative_path", relative_display},
          {"kind", kind},
          {"expandable", expandable},
          {"accessible", accessible},
          {"byte_size", byte_size},
      });
      records.push_back(std::move(record));
    }

    const auto removed = descendant_ids(workspace, directory.id);
    if (workspace.entries.size() - removed.size() + records.size() > k_maximum_workspace_entries) {
      return directory_limit();
    }
    std::ranges::sort(items, [](const json& left, const json& right) {
      const auto left_directory = left.at("kind") == "directory";
      const auto right_directory = right.at("kind") == "directory";
      if (left_directory != right_directory) return left_directory;
      return lower_ascii(left.at("name").get<std::string>())
          < lower_ascii(right.at("name").get<std::string>());
    });

    erase_descendants(workspace, directory.id, removed);
    for (auto& record : records) workspace.entries.emplace(record.id, std::move(record));
    return make_page(workspace, std::move(items), 0U, directory.id);
  }

  service_result open_document(
      const std::string_view window_session_id,
      const std::string_view workspace_id,
      const std::string_view target_kind,
      const std::string_view target_id) {
    auto workspace_iterator = workspaces_.find(std::string(window_session_id));
    if (workspace_iterator == workspaces_.end() || workspace_iterator->second.id != workspace_id) {
      return invalid_workspace();
    }
    auto& workspace = workspace_iterator->second;

    std::string document_id;
    filesystem_capability_file authorized_file;
    std::optional<document_record> pending_document;
    if (target_kind == "document") {
      const auto document_iterator = workspace.documents.find(std::string(target_id));
      if (document_iterator == workspace.documents.end()) return invalid_workspace();
      document_id = document_iterator->first;
    } else if (target_kind == "entry" && workspace.mode == "folder") {
      const auto entry_iterator = workspace.entries.find(std::string(target_id));
      if (entry_iterator == workspace.entries.end() || !entry_iterator->second.accessible
          || !entry_iterator->second.markdown || entry_iterator->second.identity.empty()) {
        return invalid_workspace();
      }
      const auto& entry = entry_iterator->second;
      auto opened = capabilities_.open_regular_file(
          workspace.root, entry.components, entry.identity);
      if (!opened.ok) return failure_for_capability(opened.error_code, "文件条目");
      authorized_file = std::move(opened.file);

      const auto existing = workspace.document_ids_by_identity.find(entry.identity);
      if (existing != workspace.document_ids_by_identity.end()) {
        document_id = existing->second;
      } else {
        if (workspace.documents.size() >= k_maximum_open_documents) {
          return service_result::failure(make_error(
              "DIRECTORY_RESOURCE_LIMIT", "当前窗口打开的文档能力已达到 64 个上限", false,
              {"REFINE_SCOPE"}));
        }
        document_record document;
        document.id = new_id("document");
        document.name = entry.name;
        document.display_path = entry.relative_display;
        document.components = entry.components;
        document.identity = entry.identity;
        document.file_version_token = new_id("version");
        document_id = document.id;
        pending_document = std::move(document);
      }
    } else {
      return invalid_workspace();
    }

    document_record* document = nullptr;
    if (pending_document) {
      document = &*pending_document;
    } else {
      auto document_iterator = workspace.documents.find(document_id);
      if (document_iterator == workspace.documents.end()) return invalid_workspace();
      document = &document_iterator->second;
    }
    file_read_result read;
    if (!authorized_file.valid()) {
      auto opened = capabilities_.open_regular_file(
          workspace.root, document->components, document->identity);
      if (!opened.ok) return failure_for_capability(opened.error_code, "文档");
      authorized_file = std::move(opened.file);
    }
    read = files_.read_markdown(std::move(authorized_file));
    if (!read.ok) return failure_for_document_read(read.error_code);
    if (!capabilities_.verify_regular_file(
            workspace.root, document->components, document->identity)) {
      return service_result::failure(make_error(
          "WORKSPACE_INVALID", "文档路径在读取期间发生变化，请刷新资源管理器", true,
          {"REOPEN_WORKSPACE"}));
    }

    document->content_hash = read.content_hash;
    document->bom = read.inspection.bom;
    document->line_ending = read.inspection.line_ending;
    document->link_count = read.link_count;
    document->read_only = read.read_only;
    document->file_version_token = new_id("version");
    if (pending_document) {
      const auto identity = document->identity;
      const auto id = document->id;
      const auto [inserted_document, inserted] = workspace.documents.emplace(id, std::move(*pending_document));
      if (!inserted) return invalid_workspace();
      const auto [identity_entry, identity_inserted] =
          workspace.document_ids_by_identity.emplace(identity, id);
      static_cast<void>(identity_entry);
      if (!identity_inserted) {
        workspace.documents.erase(inserted_document);
        return invalid_workspace();
      }
      document = &inserted_document->second;
    }
    return service_result::success_with_body(json{
        {"workspace_id", workspace.id},
        {"document_id", document->id},
        {"name", document->name},
        {"display_path", document->display_path},
        {"content_hash", document->content_hash},
        {"file_version_token", document->file_version_token},
        {"byte_size", read.byte_size},
        {"modified_time_ms", read.modified_time_ms},
        {"encoding", "utf-8"},
        {"bom", read.inspection.bom},
        {"line_ending", read.inspection.line_ending},
        {"read_only", read.read_only},
        {"resolved_from_link", document->resolved_from_link},
    }, read.content_bytes);
  }

  service_result save_document(
      const std::string_view window_session_id,
      const std::string_view workspace_id,
      const std::string_view document_id,
      const std::string_view expected_file_version_token,
      const std::string_view expected_content_hash,
      const std::uint64_t editor_revision,
      const std::string_view line_ending_policy,
      const std::span<const unsigned char> content) {
    auto workspace_iterator = workspaces_.find(std::string(window_session_id));
    if (workspace_iterator == workspaces_.end() || workspace_iterator->second.id != workspace_id) {
      return invalid_workspace();
    }
    auto& workspace = workspace_iterator->second;
    auto document_iterator = workspace.documents.find(std::string(document_id));
    if (document_iterator == workspace.documents.end()) return invalid_workspace();
    auto& document = document_iterator->second;
    if (document.file_version_token != expected_file_version_token
        || document.content_hash != expected_content_hash) {
      return failure_for_save("DOCUMENT_CONFLICT");
    }
    if (document.read_only) {
      return service_result::failure(make_error(
          "READ_ONLY", "文件是只读的，未执行保存", false, {"CHOOSE_ANOTHER"}));
    }
    if (document.resolved_from_link || document.link_count != 1U) {
      return failure_for_save("UNSAFE_FILE_METADATA");
    }

    auto serialization = serialize_markdown_for_save(
        content, document.bom, document.line_ending, line_ending_policy);
    if (!serialization.ok) {
      if (serialization.error_code == "FORMAT_DECISION_REQUIRED") {
        return service_result::failure(make_error(
            "FORMAT_DECISION_REQUIRED", "混合换行必须先明确统一为 LF 或 CRLF", false,
            {"CHOOSE_LINE_ENDING"}));
      }
      if (serialization.error_code == "CONTENT_TOO_LARGE") {
        return service_result::failure(make_error(
            "CONTENT_TOO_LARGE", "序列化后的 Markdown 超过 5 MiB", false, {"REFINE_SCOPE"}));
      }
      return service_result::failure(make_error(
          "INVALID_ENCODING", "保存正文不是规范的 UTF-8/LF 编辑器内容", false, {}));
    }
    const auto replacement_hash = loop::support::sha256_hex(serialization.bytes);
    const auto replaced = capabilities_.replace_regular_file(
        workspace.root,
        document.components,
        document.identity,
        document.content_hash,
        serialization.bytes);
    if (!replaced.ok) return failure_for_save(replaced.error_code);

    auto final_open = capabilities_.open_regular_file(
        workspace.root, document.components, replaced.identity);
    if (!final_open.ok) return failure_for_save("SAVE_OUTCOME_UNKNOWN");
    const auto final_read = files_.read_markdown(std::move(final_open.file));
    if (!final_read.ok || final_read.content_hash != replacement_hash
        || !capabilities_.verify_regular_file(
            workspace.root, document.components, replaced.identity)) {
      return failure_for_save("SAVE_OUTCOME_UNKNOWN");
    }

    const auto old_identity = document.identity;
    if (const auto new_mapping = workspace.document_ids_by_identity.find(replaced.identity);
        new_mapping != workspace.document_ids_by_identity.end()
            && new_mapping->second != document.id) {
      return failure_for_save("SAVE_OUTCOME_UNKNOWN");
    }
    if (const auto old_mapping = workspace.document_ids_by_identity.find(old_identity);
        old_mapping != workspace.document_ids_by_identity.end()
            && old_mapping->second == document.id) {
      workspace.document_ids_by_identity.erase(old_mapping);
    }
    workspace.document_ids_by_identity[replaced.identity] = document.id;
    document.identity = replaced.identity;
    document.content_hash = final_read.content_hash;
    document.file_version_token = new_id("version");
    document.bom = final_read.inspection.bom;
    document.line_ending = final_read.inspection.line_ending;
    document.link_count = final_read.link_count;
    document.read_only = final_read.read_only;
    return service_result::success(json{
        {"workspace_id", workspace.id},
        {"document_id", document.id},
        {"content_hash", document.content_hash},
        {"file_version_token", document.file_version_token},
        {"saved_revision", editor_revision},
        {"byte_size", final_read.byte_size},
        {"modified_time_ms", final_read.modified_time_ms},
        {"encoding", "utf-8"},
        {"bom", document.bom},
        {"line_ending", document.line_ending},
        {"read_only", document.read_only},
        {"resolved_from_link", document.resolved_from_link},
    });
  }

 private:
  static service_result invalid_workspace() {
    return service_result::failure(make_error(
        "WORKSPACE_INVALID", "工作区能力无效或已经撤销", true, {"REOPEN_WORKSPACE"}));
  }

  static service_result directory_limit() {
    return service_result::failure(make_error(
        "DIRECTORY_RESOURCE_LIMIT", "单个目录超过 50000 项或 32 MiB 元数据限制", false, {"REFINE_SCOPE"}));
  }

  static std::unordered_set<std::string> descendant_ids(
      const workspace_capability& workspace,
      const std::string& directory_id) {
    std::unordered_set<std::string> removed;
    bool changed = true;
    while (changed) {
      changed = false;
      for (const auto& [id, entry] : workspace.entries) {
        if (id == directory_id || removed.contains(id)) continue;
        if (entry.parent_id == directory_id || removed.contains(entry.parent_id)) {
          removed.insert(id);
          changed = true;
        }
      }
    }
    return removed;
  }

  static void erase_descendants(
      workspace_capability& workspace,
      const std::string& directory_id,
      const std::unordered_set<std::string>& removed) {
    for (const auto& id : removed) workspace.entries.erase(id);
    for (auto iterator = workspace.cursors.begin(); iterator != workspace.cursors.end();) {
      if (iterator->second.directory_id == directory_id || removed.contains(iterator->second.directory_id)) {
        iterator = workspace.cursors.erase(iterator);
      } else {
        ++iterator;
      }
    }
  }

  static service_result make_page(
      workspace_capability& workspace,
      std::vector<json> items,
      const std::size_t starting_offset,
      const std::string& directory_id) {
    const auto total_entries = items.size();
    json entries = json::array();
    std::size_t offset = starting_offset;
    std::size_t payload_bytes = 512U;
    while (offset < items.size() && entries.size() < k_maximum_entries_per_page) {
      const auto item_bytes = items[offset].dump().size();
      if (!entries.empty() && payload_bytes + item_bytes > k_maximum_page_payload_bytes) break;
      payload_bytes += item_bytes;
      entries.push_back(items[offset]);
      ++offset;
    }

    json next_cursor = nullptr;
    if (offset < items.size()) {
      if (workspace.cursors.size() >= k_maximum_active_cursors) workspace.cursors.clear();
      const auto cursor_id = new_id("cursor");
      next_cursor = cursor_id;
      workspace.cursors.emplace(cursor_id, cursor_state{directory_id, std::move(items), offset});
    }
    return service_result::success(json{
        {"workspace_id", workspace.id},
        {"directory_id", directory_id},
        {"entries", std::move(entries)},
        {"next_cursor", std::move(next_cursor)},
        {"total_entries", total_entries},
    });
  }

  file_service files_;
  filesystem_capability_port capabilities_;
  std::unordered_map<std::string, workspace_capability> workspaces_;
};

workspace_service::workspace_service() : impl_(std::make_unique<impl>()) {}
workspace_service::~workspace_service() = default;
workspace_service::workspace_service(workspace_service&&) noexcept = default;
workspace_service& workspace_service::operator=(workspace_service&&) noexcept = default;

service_result workspace_service::open_file(
    const std::string_view window_session_id,
    const std::string_view locator) {
  return impl_->open_file(window_session_id, locator);
}

service_result workspace_service::open_folder(
    const std::string_view window_session_id,
    const std::string_view locator) {
  return impl_->open_folder(window_session_id, locator);
}

service_result workspace_service::close(const std::string_view window_session_id) {
  return impl_->close(window_session_id);
}

service_result workspace_service::close_document(
    const std::string_view window_session_id,
    const std::string_view workspace_id,
    const std::string_view document_id) {
  return impl_->close_document(window_session_id, workspace_id, document_id);
}

service_result workspace_service::list_children(
    const std::string_view window_session_id,
    const std::string_view workspace_id,
    const std::string_view directory_id,
    const std::string_view cursor) {
  return impl_->list_children(window_session_id, workspace_id, directory_id, cursor);
}

service_result workspace_service::open_document(
    const std::string_view window_session_id,
    const std::string_view workspace_id,
    const std::string_view target_kind,
    const std::string_view target_id) {
  return impl_->open_document(window_session_id, workspace_id, target_kind, target_id);
}

service_result workspace_service::save_document(
    const std::string_view window_session_id,
    const std::string_view workspace_id,
    const std::string_view document_id,
    const std::string_view expected_file_version_token,
    const std::string_view expected_content_hash,
    const std::uint64_t editor_revision,
    const std::string_view line_ending_policy,
    const std::span<const unsigned char> content) {
  return impl_->save_document(
      window_session_id,
      workspace_id,
      document_id,
      expected_file_version_token,
      expected_content_hash,
      editor_revision,
      line_ending_policy,
      content);
}

}  // namespace loop::service
