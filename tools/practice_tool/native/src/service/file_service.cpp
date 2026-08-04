#include "service/file_service.h"

#include <algorithm>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "support/portable_crypto.h"

#include <uv.h>

namespace loop::service {
namespace {

class uv_file_request {
 public:
  ~uv_file_request() { uv_fs_req_cleanup(&request_); }
  uv_file_request(const uv_file_request&) = delete;
  uv_file_request& operator=(const uv_file_request&) = delete;
  uv_file_request() = default;
  [[nodiscard]] uv_fs_t* get() { return &request_; }

 private:
  uv_fs_t request_{};
};

class opened_file {
 public:
  explicit opened_file(const uv_file descriptor) : descriptor_(descriptor) {}
  ~opened_file() {
    if (descriptor_ < 0) return;
    uv_file_request request;
    static_cast<void>(uv_fs_close(nullptr, request.get(), descriptor_, nullptr));
  }
  opened_file(const opened_file&) = delete;
  opened_file& operator=(const opened_file&) = delete;
  [[nodiscard]] uv_file get() const { return descriptor_; }

 private:
  uv_file descriptor_;
};

std::string path_bytes(const std::filesystem::path& path) {
  const auto converted = path.u8string();
  return {reinterpret_cast<const char*>(converted.data()), converted.size()};
}

std::filesystem::path path_from_utf8(const std::string_view value) {
  std::u8string converted;
  converted.reserve(value.size());
  for (const char character : value) {
    converted.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
  }
  return std::filesystem::path(converted);
}

std::string identity_from_stat(const uv_stat_t& stat) {
  return std::to_string(stat.st_dev) + ':' + std::to_string(stat.st_ino);
}

std::uint64_t modified_time_ms(const uv_stat_t& stat) {
  if (stat.st_mtim.tv_sec < 0 || stat.st_mtim.tv_nsec < 0) return 0U;
  const auto seconds = static_cast<std::uint64_t>(stat.st_mtim.tv_sec);
  const auto nanoseconds = static_cast<std::uint64_t>(stat.st_mtim.tv_nsec);
  if (seconds > (std::numeric_limits<std::uint64_t>::max() - nanoseconds / 1'000'000U) / 1000U) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return seconds * 1000U + nanoseconds / 1'000'000U;
}

std::string uv_error_code(const int error) {
  if (error == UV_ENOENT || error == UV_ENOTDIR) return "NOT_FOUND";
  if (error == UV_EACCES || error == UV_EPERM) return "PERMISSION_DENIED";
  return "INTERNAL_ERROR";
}

file_read_result read_markdown_descriptor(
    const uv_file descriptor,
    const std::string_view expected_identity,
    const file_service::after_read_observer& after_read) {
  file_read_result result;
  uv_file_request before_request;
  if (uv_fs_fstat(nullptr, before_request.get(), descriptor, nullptr) < 0) {
    result.error_code = "INTERNAL_ERROR";
    return result;
  }
  const auto before = before_request.get()->statbuf;
  const auto file_type = before.st_mode & static_cast<std::uint64_t>(S_IFMT);
  if (file_type != static_cast<std::uint64_t>(S_IFREG)) {
    result.error_code = "NOT_REGULAR_FILE";
    return result;
  }
  const auto identity = identity_from_stat(before);
  if (!expected_identity.empty() && identity != expected_identity) {
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }
  if (before.st_size > k_maximum_markdown_bytes) {
    result.error_code = "CONTENT_TOO_LARGE";
    return result;
  }

  result.raw_bytes.resize(static_cast<std::size_t>(before.st_size));
  std::size_t offset = 0U;
  while (offset < result.raw_bytes.size()) {
    const auto remaining = result.raw_bytes.size() - offset;
    uv_buf_t buffer = uv_buf_init(
        reinterpret_cast<char*>(result.raw_bytes.data() + offset),
        static_cast<unsigned int>(remaining));
    uv_file_request read_request;
    const auto read_count = uv_fs_read(
        nullptr,
        read_request.get(),
        descriptor,
        &buffer,
        1U,
        static_cast<std::int64_t>(offset),
        nullptr);
    if (read_count <= 0) {
      result.error_code = "WORKSPACE_INVALID";
      return result;
    }
    offset += static_cast<std::size_t>(read_count);
  }

  if (after_read) after_read();

  uv_file_request after_request;
  if (uv_fs_fstat(nullptr, after_request.get(), descriptor, nullptr) < 0) {
    result.error_code = "INTERNAL_ERROR";
    return result;
  }
  const auto after = after_request.get()->statbuf;
  if (identity_from_stat(after) != identity || after.st_size != before.st_size
      || after.st_mtim.tv_sec != before.st_mtim.tv_sec
      || after.st_mtim.tv_nsec != before.st_mtim.tv_nsec) {
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }

  if (!inspect_markdown_bytes(result.raw_bytes, result.inspection)) {
    result.error_code = "INVALID_ENCODING";
    return result;
  }
  const auto content_offset = result.inspection.bom ? 3U : 0U;
  result.content_bytes.assign(
      result.raw_bytes.begin() + static_cast<std::ptrdiff_t>(content_offset),
      result.raw_bytes.end());
  result.content_hash = loop::support::sha256_hex(result.raw_bytes);
  result.identity = identity;
  result.byte_size = before.st_size;
  result.modified_time_ms = modified_time_ms(before);
  result.link_count = before.st_nlink;
  result.read_only = (before.st_mode & 0222U) == 0U;
  result.ok = true;
  return result;
}

}  // namespace

file_service::file_service(after_read_observer after_read)
    : after_read_(std::move(after_read)) {}

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

  std::size_t index = 0U;
  while (index < content.size()) {
    const auto first = content[index];
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
    if (index + length > content.size()) return false;
    for (std::size_t offset_index = 1U; offset_index < length; ++offset_index) {
      const auto continuation = content[index + offset_index];
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
  if (std::ranges::find(content, static_cast<unsigned char>(0U)) != content.end()) return false;

  std::size_t crlf = 0U;
  std::size_t lf = 0U;
  std::size_t bare_cr = 0U;
  for (std::size_t line_index = 0U; line_index < content.size(); ++line_index) {
    if (content[line_index] == static_cast<unsigned char>('\r')) {
      if (line_index + 1U < content.size()
          && content[line_index + 1U] == static_cast<unsigned char>('\n')) {
        ++crlf;
        ++line_index;
      } else {
        ++bare_cr;
      }
    } else if (content[line_index] == static_cast<unsigned char>('\n')) {
      ++lf;
    }
  }
  if (crlf == 0U && lf == 0U && bare_cr == 0U) inspection.line_ending = "none";
  else if (crlf > 0U && lf == 0U && bare_cr == 0U) inspection.line_ending = "crlf";
  else if (lf > 0U && crlf == 0U && bare_cr == 0U) inspection.line_ending = "lf";
  else inspection.line_ending = "mixed";
  return true;
}

file_read_result file_service::read_markdown(
    const std::filesystem::path& canonical_path,
    const std::string_view expected_identity) const {
  file_read_result result;
  std::string encoded_path;
  try {
    encoded_path = path_bytes(canonical_path);
  } catch (...) {
    result.error_code = "INTERNAL_ERROR";
    return result;
  }

  uv_file_request open_request;
  const auto descriptor = uv_fs_open(nullptr, open_request.get(), encoded_path.c_str(), O_RDONLY, 0, nullptr);
  if (descriptor < 0) {
    result.error_code = uv_error_code(descriptor);
    return result;
  }
  opened_file file(descriptor);
  result = read_markdown_descriptor(file.get(), expected_identity, after_read_);
  if (!result.ok) return result;

  uv_file_request realpath_request;
  const auto realpath_result = uv_fs_realpath(
      nullptr, realpath_request.get(), encoded_path.c_str(), nullptr);
  if (realpath_result < 0 || realpath_request.get()->ptr == nullptr) {
    result.error_code = uv_error_code(realpath_result);
    result.ok = false;
    return result;
  }
  try {
    result.canonical_path = path_from_utf8(static_cast<const char*>(realpath_request.get()->ptr));
  } catch (...) {
    result.error_code = "INTERNAL_ERROR";
    result.ok = false;
    return result;
  }

  uv_file_request final_path_request;
  if (uv_fs_stat(nullptr, final_path_request.get(), static_cast<const char*>(realpath_request.get()->ptr), nullptr) < 0
      || identity_from_stat(final_path_request.get()->statbuf) != result.identity) {
    result.error_code = "WORKSPACE_INVALID";
    result.ok = false;
    return result;
  }
  return result;
}

file_read_result file_service::read_markdown(filesystem_capability_file file) const {
  if (!file.valid()) {
    file_read_result result;
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }
  return read_markdown_descriptor(file.descriptor(), "", after_read_);
}

}  // namespace loop::service
