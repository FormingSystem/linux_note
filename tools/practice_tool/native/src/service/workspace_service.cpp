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

bool same_file(const path_info& first, const path_info& second) {
  return same_path_entry(first.canonical_path, second.canonical_path)
      && first.size == second.size
      && first.write_seconds == second.write_seconds
      && first.write_nanoseconds == second.write_nanoseconds
      && first.regular_file == second.regular_file && first.directory == second.directory;
}

struct entry_record {
  std::string id;
  std::string parent_id;
  fs::path path;
  std::string identity;
  std::string relative_display;
  bool directory = false;
  bool accessible = false;
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
  fs::path canonical_root;
  std::unordered_map<std::string, entry_record> entries;
  std::unordered_map<std::string, cursor_state> cursors;
  std::string selected_document_id;
  std::string selected_file_identity;
  std::string selected_file_hash;
};

}  // namespace

service_result service_result::success(json value) {
  service_result result;
  result.ok = true;
  result.value = std::move(value);
  return result;
}

service_result service_result::failure(service_error error) {
  service_result result;
  result.error = std::move(error);
  return result;
}

bool inspect_markdown_bytes(
    const std::span<const unsigned char> bytes,
    markdown_inspection& inspection) {
  inspection = {};
  std::size_t offset = 0U;
  if (bytes.size() >= 3U && bytes[0] == 0xEFU && bytes[1] == 0xBBU && bytes[2] == 0xBFU) {
    inspection.bom = true;
    offset = 3U;
  }
  const auto content = bytes.subspan(offset);
  if (!valid_utf8(content)) return false;
  if (std::ranges::find(content, static_cast<unsigned char>(0U)) != content.end()) return false;

  std::size_t crlf = 0U;
  std::size_t lf = 0U;
  std::size_t bare_cr = 0U;
  for (std::size_t index = 0U; index < content.size(); ++index) {
    if (content[index] == static_cast<unsigned char>('\r')) {
      if (index + 1U < content.size() && content[index + 1U] == static_cast<unsigned char>('\n')) {
        ++crlf;
        ++index;
      } else {
        ++bare_cr;
      }
    } else if (content[index] == static_cast<unsigned char>('\n')) {
      ++lf;
    }
  }
  if (crlf == 0U && lf == 0U && bare_cr == 0U) inspection.line_ending = "none";
  else if (crlf > 0U && lf == 0U && bare_cr == 0U) inspection.line_ending = "crlf";
  else if (lf > 0U && crlf == 0U && bare_cr == 0U) inspection.line_ending = "lf";
  else inspection.line_ending = "mixed";
  return true;
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
    if (initial->size > k_maximum_markdown_bytes) {
      return service_result::failure(make_error(
          "CONTENT_TOO_LARGE", "Markdown 文件超过 5 MiB 首版限制", false, {"CHOOSE_ANOTHER"}));
    }

    std::ifstream input(initial->canonical_path, std::ios::binary);
    if (!input) return failure_for_path("PERMISSION_DENIED", "文件");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(initial->size));
    if (!bytes.empty()) {
      input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        return service_result::failure(make_error(
            "WORKSPACE_INVALID", "文件在打开期间发生变化，请重试", true, {"RETRY", "CHOOSE_ANOTHER"}));
      }
    }
    markdown_inspection inspection;
    if (!inspect_markdown_bytes(bytes, inspection)) {
      return service_result::failure(make_error(
          "INVALID_ENCODING", "文件不是有效的 UTF-8 Markdown，已拒绝替换字符解码", false, {"CHOOSE_ANOTHER"}));
    }

    const auto content_hash = loop::support::sha256_hex(bytes);
    const auto final = query_path(initial->canonical_path, failure_code);
    if (!final || !same_file(*initial, *final)) {
      return service_result::failure(make_error(
          "WORKSPACE_INVALID", "文件在打开期间发生变化，请重试", true, {"RETRY", "CHOOSE_ANOTHER"}));
    }

    workspace_capability workspace;
    workspace.id = new_id("workspace");
    workspace.window_session_id = std::string(window_session_id);
    workspace.mode = "single_file";
    workspace.canonical_root = final->canonical_path.parent_path();
    workspace.selected_file_identity = final->identity;
    workspace.selected_file_hash = content_hash;

    workspace.selected_document_id = new_id("document");
    const auto document_id = workspace.selected_document_id;
    const auto display_name = safe_name(final->canonical_path);
    const auto resolved_from_link = path_was_link(requested);
    const auto result = json{
        {"workspace_id", workspace.id},
        {"mode", "single_file"},
        {"display_name", display_name},
        {"document", {
            {"document_id", document_id},
            {"name", display_name},
            {"display_path", display_name},
            {"byte_size", final->size},
            {"encoding", "utf-8"},
            {"bom", inspection.bom},
            {"line_ending", inspection.line_ending},
            {"read_only", final->read_only},
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

    std::string failure_code;
    const auto info = query_path(requested, failure_code);
    if (!info) return failure_for_path(failure_code, "文件夹");
    if (!info->directory) {
      return service_result::failure(make_error(
          "NOT_DIRECTORY", "所选目标不是文件夹", false, {"CHOOSE_ANOTHER"}));
    }

    workspace_capability workspace;
    workspace.id = new_id("workspace");
    workspace.window_session_id = std::string(window_session_id);
    workspace.mode = "folder";
    workspace.canonical_root = info->canonical_path;

    entry_record root;
    root.id = new_id("directory");
    root.path = info->canonical_path;
    root.identity = info->identity;
    root.directory = true;
    root.accessible = true;
    const auto root_id = root.id;
    workspace.entries.emplace(root.id, std::move(root));

    const auto display_name = safe_name(info->canonical_path);
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
    std::string failure_code;
    const auto current = query_path(directory.path, failure_code);
    if (!current || !current->directory || current->identity != directory.identity
        || !path_is_within(workspace.canonical_root, current->canonical_path)) {
      return service_result::failure(make_error(
          "WORKSPACE_INVALID", "文件夹身份已经变化，请重新打开工作区", true, {"REOPEN_WORKSPACE"}));
    }

    std::error_code enumeration_error;
    fs::directory_iterator iterator(current->canonical_path, enumeration_error);
    if (enumeration_error) return failure_for_path("PERMISSION_DENIED", "文件夹");

    std::vector<json> items;
    std::vector<entry_record> records;
    std::size_t metadata_bytes = 0U;
    const fs::directory_iterator end;
    for (; iterator != end; iterator.increment(enumeration_error)) {
      if (enumeration_error) return failure_for_path("PERMISSION_DENIED", "文件夹");
      const auto& item = *iterator;
      if (items.size() >= k_maximum_directory_entries) return directory_limit();
      std::string raw_name;
      bool name_valid = false;
      try {
        raw_name = path_bytes(item.path().filename());
        name_valid = valid_utf8(std::span(
            reinterpret_cast<const unsigned char*>(raw_name.data()), raw_name.size()));
      } catch (...) {
        raw_name = "<无法显示的名称>";
      }
      const auto name = escaped_display_name(raw_name);
      const auto relative_display = join_display_path(directory.relative_display, name);
      metadata_bytes += name.size() + relative_display.size() + 256U;
      if (metadata_bytes > k_maximum_directory_metadata_bytes) return directory_limit();

      entry_record record;
      record.id = new_id("entry");
      record.parent_id = directory.id;
      record.path = item.path();
      record.relative_display = relative_display;

      std::string kind = "other";
      bool expandable = false;
      bool accessible = false;
      json byte_size = nullptr;
      std::error_code status_error;
      const auto status = item.symlink_status(status_error);
      const auto link = !status_error && path_was_link(item.path());
      if (!status_error && link) {
        kind = "symbolic_link";
      } else if (!status_error && fs::is_directory(status)) {
        const auto child = query_path(item.path(), failure_code);
        if (child && child->directory
            && path_is_within(workspace.canonical_root, child->canonical_path) && name_valid) {
          kind = "directory";
          expandable = true;
          accessible = true;
          record.path = child->canonical_path;
          record.identity = child->identity;
          record.directory = true;
          record.accessible = true;
        }
      } else if (!status_error && fs::is_regular_file(status)) {
        std::error_code size_error;
        const auto file_size = item.file_size(size_error);
        const auto child = !size_error ? query_path(item.path(), failure_code) : std::nullopt;
        if (child && child->regular_file && path_is_within(workspace.canonical_root, child->canonical_path)
            && name_valid) {
          kind = is_markdown_path(item.path()) ? "markdown" : "file";
          accessible = true;
          record.accessible = true;
          if (file_size <= static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max())) {
            byte_size = file_size;
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
    if (enumeration_error) return failure_for_path("PERMISSION_DENIED", "文件夹");

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

service_result workspace_service::list_children(
    const std::string_view window_session_id,
    const std::string_view workspace_id,
    const std::string_view directory_id,
    const std::string_view cursor) {
  return impl_->list_children(window_session_id, workspace_id, directory_id, cursor);
}

}  // namespace loop::service
