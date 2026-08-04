#include "service/filesystem_capability_port.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <utility>

#include <uv.h>

#include "support/portable_crypto.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <aclapi.h>
#include <winternl.h>
#elif defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>
#endif

namespace loop::service {
namespace {

constexpr std::size_t k_directory_buffer_bytes = 64U * 1024U;
constexpr std::size_t k_entry_metadata_overhead = 256U;
constexpr std::size_t k_maximum_capability_file_bytes = 5U * 1024U * 1024U;

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

std::string escaped_display_name(const std::string_view bytes) {
  const auto byte_span = std::span(
      reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
  if (!bytes.empty() && bytes.size() <= 1024U && valid_utf8(byte_span)) {
    return std::string(bytes);
  }

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

bool safe_component_chain(const filesystem_component_chain& components) {
  return std::ranges::all_of(components, [](const std::string& component) {
    return is_safe_native_capability_child_name(component);
  });
}

void close_uv_descriptor(const int descriptor) {
  if (descriptor < 0) return;
  uv_fs_t request{};
  static_cast<void>(uv_fs_close(nullptr, &request, descriptor, nullptr));
  uv_fs_req_cleanup(&request);
}

#if defined(_WIN32)

class windows_handle_guard {
 public:
  explicit windows_handle_guard(const HANDLE handle = INVALID_HANDLE_VALUE) : handle_(handle) {}
  ~windows_handle_guard() {
    if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr) CloseHandle(handle_);
  }
  windows_handle_guard(const windows_handle_guard&) = delete;
  windows_handle_guard& operator=(const windows_handle_guard&) = delete;
  windows_handle_guard(windows_handle_guard&& other) noexcept : handle_(other.release()) {}
  windows_handle_guard& operator=(windows_handle_guard&& other) noexcept {
    if (this == &other) return *this;
    if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr) CloseHandle(handle_);
    handle_ = other.release();
    return *this;
  }
  [[nodiscard]] HANDLE get() const { return handle_; }
  [[nodiscard]] HANDLE release() {
    const auto handle = handle_;
    handle_ = INVALID_HANDLE_VALUE;
    return handle;
  }

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

bool file_id_is_zero(const FILE_ID_128& file_id) {
  return std::ranges::all_of(file_id.Identifier, [](const unsigned char byte) { return byte == 0U; });
}

std::string windows_identity(const std::uint64_t volume, const FILE_ID_128& file_id) {
  constexpr char hex[] = "0123456789abcdef";
  std::string value = std::to_string(volume) + ':';
  value.reserve(value.size() + 32U);
  for (const unsigned char byte : file_id.Identifier) {
    value.push_back(hex[(byte >> 4U) & 0x0FU]);
    value.push_back(hex[byte & 0x0FU]);
  }
  return value;
}

bool query_windows_identity(
    const HANDLE handle,
    std::string& identity,
    std::uint64_t& volume) {
  FILE_ID_INFO info{};
  if (GetFileInformationByHandleEx(handle, FileIdInfo, &info, sizeof(info)) == FALSE
      || file_id_is_zero(info.FileId)) {
    return false;
  }
  volume = info.VolumeSerialNumber;
  identity = windows_identity(volume, info.FileId);
  return true;
}

bool windows_handle_is_reparse_point(const HANDLE handle, bool& is_reparse) {
  FILE_ATTRIBUTE_TAG_INFO info{};
  if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info)) == FALSE) {
    return false;
  }
  is_reparse = (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
  return true;
}

enum class windows_remote_state {
  local,
  remote,
  unsupported,
};

windows_remote_state query_windows_remote_state(const HANDLE handle) {
  FILE_REMOTE_PROTOCOL_INFO info{};
  if (GetFileInformationByHandleEx(handle, FileRemoteProtocolInfo, &info, sizeof(info)) == FALSE) {
    return windows_remote_state::unsupported;
  }
  return info.Protocol == 0U ? windows_remote_state::local : windows_remote_state::remote;
}

windows_remote_state selected_windows_root_state(const std::filesystem::path& path) {
  try {
    const auto selected_locator = path.wstring();
    const auto extended_unc = selected_locator.starts_with(L"\\\\?\\UNC\\");
    const auto ordinary_unc = selected_locator.starts_with(L"\\\\")
        && !selected_locator.starts_with(L"\\\\?\\")
        && !selected_locator.starts_with(L"\\\\.\\");
    if (extended_unc || ordinary_unc) {
      return windows_remote_state::remote;
    }
    const auto absolute_path = std::filesystem::absolute(path).wstring();
    const auto root_path = std::filesystem::path(absolute_path).root_path().wstring();
    if (root_path.empty()) return windows_remote_state::unsupported;
    const auto drive_type = GetDriveTypeW(root_path.c_str());
    if (drive_type == DRIVE_REMOTE) return windows_remote_state::remote;
    if (drive_type == DRIVE_UNKNOWN || drive_type == DRIVE_NO_ROOT_DIR) {
      return windows_remote_state::unsupported;
    }
    return windows_remote_state::local;
  } catch (...) {
    return windows_remote_state::unsupported;
  }
}

std::wstring extended_windows_path(const std::filesystem::path& path) {
  auto value = std::filesystem::absolute(path).wstring();
  if (value.starts_with(L"\\\\?\\")) return value;
  if (value.starts_with(L"\\\\")) return L"\\\\?\\UNC\\" + value.substr(2U);
  return L"\\\\?\\" + value;
}

bool utf8_to_wide(const std::string_view value, std::wstring& converted) {
  if (value.empty() || value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return false;
  }
  const auto length = MultiByteToWideChar(
      CP_UTF8,
      MB_ERR_INVALID_CHARS,
      value.data(),
      static_cast<int>(value.size()),
      nullptr,
      0);
  if (length <= 0) return false;
  converted.resize(static_cast<std::size_t>(length));
  return MultiByteToWideChar(
      CP_UTF8,
      MB_ERR_INVALID_CHARS,
      value.data(),
      static_cast<int>(value.size()),
      converted.data(),
      length) == length;
}

bool wide_to_utf8(const std::wstring_view value, std::string& converted) {
  if (value.empty() || value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return false;
  }
  const auto length = WideCharToMultiByte(
      CP_UTF8,
      WC_ERR_INVALID_CHARS,
      value.data(),
      static_cast<int>(value.size()),
      nullptr,
      0,
      nullptr,
      nullptr);
  if (length <= 0) return false;
  converted.resize(static_cast<std::size_t>(length));
  return WideCharToMultiByte(
      CP_UTF8,
      WC_ERR_INVALID_CHARS,
      value.data(),
      static_cast<int>(value.size()),
      converted.data(),
      length,
      nullptr,
      nullptr) == length;
}

bool windows_relative_name(
    const filesystem_component_chain& components,
    std::wstring& relative_name) {
  if (components.empty() || !safe_component_chain(components)) return false;
  relative_name.clear();
  for (const auto& component : components) {
    std::wstring converted;
    if (!utf8_to_wide(component, converted)) return false;
    if (!relative_name.empty()) relative_name.push_back(L'\\');
    if (relative_name.size() + converted.size() > 32'766U) return false;
    relative_name.append(converted);
  }
  return !relative_name.empty()
      && relative_name.size() <= static_cast<std::size_t>(std::numeric_limits<USHORT>::max() / 2U);
}

std::string windows_error_code(const ULONG error) {
  if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
      || error == ERROR_INVALID_NAME || error == ERROR_DIRECTORY) {
    return "NOT_FOUND";
  }
  if (error == ERROR_ACCESS_DENIED || error == ERROR_PRIVILEGE_NOT_HELD
      || error == ERROR_SHARING_VIOLATION) {
    return "PERMISSION_DENIED";
  }
  if (error == ERROR_REPARSE_POINT_ENCOUNTERED || error == ERROR_CANT_ACCESS_FILE) {
    return "LINK_OR_MOUNT_BLOCKED";
  }
  if (error == ERROR_NOT_SUPPORTED || error == ERROR_INVALID_FUNCTION
      || error == ERROR_CALL_NOT_IMPLEMENTED) {
    return "PLATFORM_UNSUPPORTED";
  }
  return "WORKSPACE_INVALID";
}

struct windows_relative_open_result {
  bool ok = false;
  std::string error_code;
  std::string identity;
  windows_handle_guard handle;
};

windows_relative_open_result open_windows_relative(
    const HANDLE root_handle,
    const std::uint64_t root_volume,
    const filesystem_component_chain& components,
    const filesystem_capability_entry_kind expected_kind,
    const std::string_view expected_identity,
    const ACCESS_MASK additional_access = 0U) {
  windows_relative_open_result result;
  std::wstring relative_name;
  if (!windows_relative_name(components, relative_name)) {
    result.error_code = "INVALID_REQUEST";
    return result;
  }

  UNICODE_STRING unicode_name{};
  unicode_name.Length = static_cast<USHORT>(relative_name.size() * sizeof(wchar_t));
  unicode_name.MaximumLength = unicode_name.Length;
  unicode_name.Buffer = relative_name.data();
  OBJECT_ATTRIBUTES attributes{};
  InitializeObjectAttributes(
      &attributes,
      &unicode_name,
      OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE,
      root_handle,
      nullptr);
  IO_STATUS_BLOCK status_block{};
  HANDLE opened_handle = INVALID_HANDLE_VALUE;
  const auto directory = expected_kind == filesystem_capability_entry_kind::directory;
  const auto desired_access = static_cast<ACCESS_MASK>(
      (directory ? FILE_LIST_DIRECTORY : FILE_READ_DATA)
      | FILE_READ_ATTRIBUTES | SYNCHRONIZE | additional_access);
  const auto create_options = static_cast<ULONG>(
      FILE_SYNCHRONOUS_IO_NONALERT | (directory ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE));
  const auto status = NtCreateFile(
      &opened_handle,
      desired_access,
      &attributes,
      &status_block,
      nullptr,
      FILE_ATTRIBUTE_NORMAL,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      FILE_OPEN,
      create_options,
      nullptr,
      0U);
  if (status < 0) {
    result.error_code = windows_error_code(RtlNtStatusToDosError(status));
    return result;
  }
  result.handle = windows_handle_guard(opened_handle);

  bool is_reparse = false;
  std::uint64_t volume = 0U;
  if (!windows_handle_is_reparse_point(result.handle.get(), is_reparse)
      || is_reparse
      || !query_windows_identity(result.handle.get(), result.identity, volume)) {
    result.error_code = "PLATFORM_UNSUPPORTED";
    return result;
  }
  if (volume != root_volume || (!expected_identity.empty() && result.identity != expected_identity)) {
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }
  result.ok = true;
  return result;
}

filesystem_capability_entry_kind windows_entry_kind(const ULONG attributes) {
  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
    return filesystem_capability_entry_kind::symbolic_link;
  }
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
    return filesystem_capability_entry_kind::directory;
  }
  if ((attributes & FILE_ATTRIBUTE_DEVICE) == 0U) {
    return filesystem_capability_entry_kind::regular_file;
  }
  return filesystem_capability_entry_kind::other;
}

std::string windows_save_error_code(const DWORD error) {
  if (error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL) return "DISK_FULL";
  if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) return "FILE_BUSY";
  if (error == ERROR_ACCESS_DENIED || error == ERROR_PRIVILEGE_NOT_HELD) return "PERMISSION_DENIED";
  return windows_error_code(error);
}

bool windows_regular_file_hash(
    const HANDLE handle,
    std::string& hash,
    std::uint64_t& link_count,
    std::string& error_code) {
  FILE_STANDARD_INFO before{};
  FILE_BASIC_INFO before_basic{};
  if (GetFileInformationByHandleEx(handle, FileStandardInfo, &before, sizeof(before)) == FALSE
      || GetFileInformationByHandleEx(handle, FileBasicInfo, &before_basic, sizeof(before_basic)) == FALSE
      || before.Directory != FALSE || before.EndOfFile.QuadPart < 0
      || static_cast<std::uint64_t>(before.EndOfFile.QuadPart) > k_maximum_capability_file_bytes) {
    error_code = "WORKSPACE_INVALID";
    return false;
  }
  std::vector<unsigned char> bytes(static_cast<std::size_t>(before.EndOfFile.QuadPart));
  LARGE_INTEGER zero{};
  if (SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN) == FALSE) {
    error_code = windows_save_error_code(GetLastError());
    return false;
  }
  std::size_t offset = 0U;
  while (offset < bytes.size()) {
    DWORD read_count = 0U;
    const auto remaining = std::min<std::size_t>(
        bytes.size() - offset,
        static_cast<std::size_t>(std::numeric_limits<DWORD>::max()));
    if (ReadFile(
            handle,
            bytes.data() + offset,
            static_cast<DWORD>(remaining),
            &read_count,
            nullptr) == FALSE
        || read_count == 0U) {
      error_code = windows_save_error_code(GetLastError());
      return false;
    }
    offset += read_count;
  }
  FILE_STANDARD_INFO after{};
  FILE_BASIC_INFO after_basic{};
  if (GetFileInformationByHandleEx(handle, FileStandardInfo, &after, sizeof(after)) == FALSE
      || GetFileInformationByHandleEx(handle, FileBasicInfo, &after_basic, sizeof(after_basic)) == FALSE
      || after.EndOfFile.QuadPart != before.EndOfFile.QuadPart
      || after_basic.LastWriteTime.QuadPart != before_basic.LastWriteTime.QuadPart
      || after_basic.ChangeTime.QuadPart != before_basic.ChangeTime.QuadPart) {
    error_code = "DOCUMENT_CONFLICT";
    return false;
  }
  link_count = before.NumberOfLinks;
  hash = loop::support::sha256_hex(bytes);
  return true;
}

bool windows_has_only_default_stream(const HANDLE handle) {
  std::array<unsigned char, 64U * 1024U> buffer{};
  if (GetFileInformationByHandleEx(
          handle, FileStreamInfo, buffer.data(), static_cast<DWORD>(buffer.size())) == FALSE) {
    return false;
  }
  std::size_t offset = 0U;
  while (true) {
    constexpr auto header_bytes = offsetof(FILE_STREAM_INFO, StreamName);
    if (offset > buffer.size() - header_bytes) return false;
    const auto* info = reinterpret_cast<const FILE_STREAM_INFO*>(buffer.data() + offset);
    if ((info->StreamNameLength % sizeof(wchar_t)) != 0U
        || info->StreamNameLength > buffer.size() - offset - header_bytes) {
      return false;
    }
    const auto name = std::wstring_view(
        info->StreamName,
        static_cast<std::size_t>(info->StreamNameLength / sizeof(wchar_t)));
    if (name != L"::$DATA") return false;
    if (info->NextEntryOffset == 0U) return true;
    if (info->NextEntryOffset < header_bytes || info->NextEntryOffset > buffer.size() - offset) {
      return false;
    }
    offset += info->NextEntryOffset;
  }
}

void mark_windows_file_for_delete(const HANDLE handle) {
  FILE_DISPOSITION_INFO disposition{};
  disposition.DeleteFile = TRUE;
  static_cast<void>(SetFileInformationByHandle(
      handle, FileDispositionInfo, &disposition, sizeof(disposition)));
}

windows_handle_guard create_windows_temporary_file(
    const HANDLE parent_handle,
    std::wstring& temporary_name,
    std::string& error_code) {
  for (int attempt = 0; attempt < 8; ++attempt) {
    temporary_name = L".loop-save-";
    const auto suffix = loop::support::secure_random_hex(16U);
    temporary_name.append(suffix.begin(), suffix.end());
    temporary_name.append(L".tmp");
    UNICODE_STRING unicode_name{};
    unicode_name.Length = static_cast<USHORT>(temporary_name.size() * sizeof(wchar_t));
    unicode_name.MaximumLength = unicode_name.Length;
    unicode_name.Buffer = temporary_name.data();
    OBJECT_ATTRIBUTES attributes{};
    InitializeObjectAttributes(
        &attributes, &unicode_name, OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE, parent_handle, nullptr);
    IO_STATUS_BLOCK status_block{};
    HANDLE opened = INVALID_HANDLE_VALUE;
    const auto status = NtCreateFile(
        &opened,
        FILE_READ_DATA | FILE_WRITE_DATA | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES
            | READ_CONTROL | WRITE_DAC | WRITE_OWNER | DELETE | SYNCHRONIZE,
        &attributes,
        &status_block,
        nullptr,
        FILE_ATTRIBUTE_TEMPORARY,
        0U,
        FILE_CREATE,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        nullptr,
        0U);
    if (status >= 0) return windows_handle_guard(opened);
    const auto error = RtlNtStatusToDosError(status);
    if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
      error_code = windows_save_error_code(error);
      return windows_handle_guard();
    }
  }
  error_code = "INTERNAL_ERROR";
  return windows_handle_guard();
}

bool write_windows_file(
    const HANDLE handle,
    const std::span<const unsigned char> bytes,
    std::string& error_code) {
  std::size_t offset = 0U;
  while (offset < bytes.size()) {
    DWORD written = 0U;
    const auto remaining = std::min<std::size_t>(
        bytes.size() - offset,
        static_cast<std::size_t>(std::numeric_limits<DWORD>::max()));
    if (WriteFile(
            handle,
            bytes.data() + offset,
            static_cast<DWORD>(remaining),
            &written,
            nullptr) == FALSE
        || written == 0U) {
      error_code = windows_save_error_code(GetLastError());
      return false;
    }
    offset += written;
  }
  if (FlushFileBuffers(handle) == FALSE) {
    error_code = windows_save_error_code(GetLastError());
    return false;
  }
  return true;
}

bool copy_windows_metadata(
    const HANDLE source,
    const HANDLE destination,
    std::string& error_code) {
  FILE_BASIC_INFO basic{};
  if (GetFileInformationByHandleEx(source, FileBasicInfo, &basic, sizeof(basic)) == FALSE) {
    error_code = "UNSAFE_FILE_METADATA";
    return false;
  }
  constexpr DWORD unsupported_attributes = FILE_ATTRIBUTE_COMPRESSED
      | FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_SPARSE_FILE;
  if ((basic.FileAttributes & unsupported_attributes) != 0U
      || !windows_has_only_default_stream(source)) {
    error_code = "UNSAFE_FILE_METADATA";
    return false;
  }

  PSID owner = nullptr;
  PSID group = nullptr;
  PACL dacl = nullptr;
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  const auto security_result = GetSecurityInfo(
      source,
      SE_FILE_OBJECT,
      OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
      &owner,
      &group,
      &dacl,
      nullptr,
      &descriptor);
  if (security_result != ERROR_SUCCESS) {
    error_code = "UNSAFE_FILE_METADATA";
    return false;
  }
  const auto set_security_result = SetSecurityInfo(
      destination,
      SE_FILE_OBJECT,
      OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
      owner,
      group,
      dacl,
      nullptr);
  LocalFree(descriptor);
  if (set_security_result != ERROR_SUCCESS) {
    error_code = "UNSAFE_FILE_METADATA";
    return false;
  }

  FILE_BASIC_INFO destination_basic{};
  destination_basic.CreationTime = basic.CreationTime;
  destination_basic.LastAccessTime = basic.LastAccessTime;
  destination_basic.FileAttributes = basic.FileAttributes;
  if (SetFileInformationByHandle(
          destination, FileBasicInfo, &destination_basic, sizeof(destination_basic)) == FALSE) {
    error_code = "UNSAFE_FILE_METADATA";
    return false;
  }
  return true;
}

#elif defined(__linux__)

std::string path_bytes(const std::filesystem::path& path) {
  const auto converted = path.u8string();
  return {reinterpret_cast<const char*>(converted.data()), converted.size()};
}

std::string linux_identity(const struct stat& status) {
  return std::to_string(static_cast<std::uint64_t>(status.st_dev)) + ':'
      + std::to_string(static_cast<std::uint64_t>(status.st_ino));
}

std::string linux_error_code(const int error) {
  if (error == ENOENT || error == ENOTDIR) return "NOT_FOUND";
  if (error == EACCES || error == EPERM) return "PERMISSION_DENIED";
  if (error == ELOOP || error == EXDEV) return "LINK_OR_MOUNT_BLOCKED";
  if (error == ENOSYS || error == EINVAL || error == E2BIG) return "PLATFORM_UNSUPPORTED";
  return "WORKSPACE_INVALID";
}

bool linux_relative_name(
    const filesystem_component_chain& components,
    std::string& relative_name) {
  if (components.empty() || !safe_component_chain(components)) return false;
  relative_name.clear();
  for (const auto& component : components) {
    if (!relative_name.empty()) relative_name.push_back('/');
    if (relative_name.size() + component.size() > 4096U) return false;
    relative_name.append(component);
  }
  return !relative_name.empty();
}

struct linux_relative_open_result {
  bool ok = false;
  int descriptor = -1;
  std::string error_code;
  std::string identity;
};

linux_relative_open_result open_linux_relative(
    const int root_descriptor,
    const filesystem_component_chain& components,
    const filesystem_capability_entry_kind expected_kind,
    const std::string_view expected_identity) {
  linux_relative_open_result result;
  std::string relative_name;
  if (!linux_relative_name(components, relative_name)) {
    result.error_code = "INVALID_REQUEST";
    return result;
  }
#if defined(SYS_openat2)
  struct open_how how{};
  how.flags = static_cast<std::uint64_t>(O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (expected_kind == filesystem_capability_entry_kind::directory) {
    how.flags |= static_cast<std::uint64_t>(O_DIRECTORY);
  }
  how.resolve = RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_SYMLINKS | RESOLVE_NO_XDEV;
  constexpr int maximum_attempts = 3;
  for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
    const auto opened = static_cast<int>(syscall(
        SYS_openat2,
        root_descriptor,
        relative_name.c_str(),
        &how,
        sizeof(how)));
    if (opened >= 0) {
      result.descriptor = opened;
      break;
    }
    if (errno != EAGAIN || attempt + 1 == maximum_attempts) {
      result.error_code = linux_error_code(errno);
      return result;
    }
  }
#else
  result.error_code = "PLATFORM_UNSUPPORTED";
  return result;
#endif

  struct stat status{};
  if (fstat(result.descriptor, &status) != 0) {
    result.error_code = linux_error_code(errno);
    close_uv_descriptor(result.descriptor);
    result.descriptor = -1;
    return result;
  }
  const auto correct_type = expected_kind == filesystem_capability_entry_kind::directory
      ? S_ISDIR(status.st_mode)
      : S_ISREG(status.st_mode);
  result.identity = linux_identity(status);
  if (!correct_type || (!expected_identity.empty() && result.identity != expected_identity)) {
    result.error_code = "WORKSPACE_INVALID";
    close_uv_descriptor(result.descriptor);
    result.descriptor = -1;
    return result;
  }
  result.ok = true;
  return result;
}

bool linux_openat2_supported(const int root_descriptor) {
#if defined(SYS_openat2)
  struct open_how how{};
  how.flags = static_cast<std::uint64_t>(O_RDONLY | O_CLOEXEC | O_DIRECTORY);
  how.resolve = RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_SYMLINKS | RESOLVE_NO_XDEV;
  const auto descriptor = static_cast<int>(syscall(SYS_openat2, root_descriptor, ".", &how, sizeof(how)));
  if (descriptor < 0) return false;
  close_uv_descriptor(descriptor);
  return true;
#else
  static_cast<void>(root_descriptor);
  return false;
#endif
}

std::string linux_save_error_code(const int error) {
  if (error == ENOSPC || error == EDQUOT) return "DISK_FULL";
  if (error == EBUSY || error == ETXTBSY) return "FILE_BUSY";
  return linux_error_code(error);
}

bool linux_regular_file_hash(
    const int descriptor,
    std::string& hash,
    std::uint64_t& link_count,
    std::string& error_code) {
  struct stat before{};
  if (fstat(descriptor, &before) != 0 || !S_ISREG(before.st_mode)
      || before.st_size < 0
      || static_cast<std::uint64_t>(before.st_size) > k_maximum_capability_file_bytes) {
    error_code = "WORKSPACE_INVALID";
    return false;
  }
  std::vector<unsigned char> bytes(static_cast<std::size_t>(before.st_size));
  std::size_t offset = 0U;
  while (offset < bytes.size()) {
    const auto count = pread(
        descriptor,
        bytes.data() + offset,
        bytes.size() - offset,
        static_cast<off_t>(offset));
    if (count <= 0) {
      error_code = linux_save_error_code(errno);
      return false;
    }
    offset += static_cast<std::size_t>(count);
  }
  struct stat after{};
  if (fstat(descriptor, &after) != 0
      || after.st_size != before.st_size
      || after.st_mtim.tv_sec != before.st_mtim.tv_sec
      || after.st_mtim.tv_nsec != before.st_mtim.tv_nsec
      || after.st_ctim.tv_sec != before.st_ctim.tv_sec
      || after.st_ctim.tv_nsec != before.st_ctim.tv_nsec) {
    error_code = "DOCUMENT_CONFLICT";
    return false;
  }
  link_count = static_cast<std::uint64_t>(before.st_nlink);
  hash = loop::support::sha256_hex(bytes);
  return true;
}

bool copy_linux_xattrs(const int source, const int destination) {
  const auto list_size = flistxattr(source, nullptr, 0U);
  if (list_size < 0 || list_size > 64 * 1024) return false;
  if (list_size == 0) return true;
  std::vector<char> names(static_cast<std::size_t>(list_size));
  if (flistxattr(source, names.data(), names.size()) != list_size) return false;
  std::size_t offset = 0U;
  while (offset < names.size()) {
    const auto* name = names.data() + offset;
    const auto maximum_length = names.size() - offset;
    const auto length = strnlen(name, maximum_length);
    if (length == 0U || length == maximum_length) return false;
    const auto value_size = fgetxattr(source, name, nullptr, 0U);
    if (value_size < 0 || value_size > 1024 * 1024) return false;
    std::vector<unsigned char> value(static_cast<std::size_t>(value_size));
    if (value_size > 0
        && fgetxattr(source, name, value.data(), value.size()) != value_size) {
      return false;
    }
    const auto existing_size = fgetxattr(destination, name, nullptr, 0U);
    if (existing_size == value_size && existing_size >= 0) {
      std::vector<unsigned char> existing(static_cast<std::size_t>(existing_size));
      if ((existing_size == 0
              || fgetxattr(destination, name, existing.data(), existing.size()) == existing_size)
          && existing == value) {
        offset += length + 1U;
        continue;
      }
    }
    if (fsetxattr(destination, name, value.data(), value.size(), 0) != 0) return false;
    offset += length + 1U;
  }
  return true;
}

bool copy_linux_metadata(const int source, const int destination) {
  struct stat status{};
  if (fstat(source, &status) != 0 || status.st_nlink != 1U) return false;
  if (fchown(destination, status.st_uid, status.st_gid) != 0) return false;
  if (fchmod(destination, status.st_mode & 07777U) != 0) return false;
  return copy_linux_xattrs(source, destination);
}

bool write_linux_file(
    const int descriptor,
    const std::span<const unsigned char> bytes,
    std::string& error_code) {
  std::size_t offset = 0U;
  while (offset < bytes.size()) {
    const auto count = write(descriptor, bytes.data() + offset, bytes.size() - offset);
    if (count <= 0) {
      error_code = linux_save_error_code(errno);
      return false;
    }
    offset += static_cast<std::size_t>(count);
  }
  if (fsync(descriptor) != 0) {
    error_code = linux_save_error_code(errno);
    return false;
  }
  return true;
}

#endif

}  // namespace

class filesystem_workspace_root::impl {
 public:
#if defined(_WIN32)
  HANDLE handle = INVALID_HANDLE_VALUE;
  HANDLE write_handle = INVALID_HANDLE_VALUE;
  std::uint64_t volume = 0U;
#elif defined(__linux__)
  int descriptor = -1;
#endif
  std::string identity;
};

bool is_safe_capability_child_name(
    const std::string_view child_name,
    const filesystem_capability_platform platform) {
  if (child_name.empty() || child_name == "." || child_name == "..") return false;
  if (std::ranges::find(child_name, '\0') != child_name.end()) return false;
  if (std::ranges::find(child_name, '/') != child_name.end()) return false;
  if (platform == filesystem_capability_platform::windows) {
    if (std::ranges::find(child_name, '\\') != child_name.end()) return false;
    if (std::ranges::find(child_name, ':') != child_name.end()) return false;
  }
  return true;
}

bool is_safe_native_capability_child_name(const std::string_view child_name) {
#if defined(_WIN32)
  constexpr auto platform = filesystem_capability_platform::windows;
#elif defined(__linux__)
  constexpr auto platform = filesystem_capability_platform::linux;
#else
  return false;
#endif
  return is_safe_capability_child_name(child_name, platform);
}

filesystem_workspace_root::filesystem_workspace_root() = default;

filesystem_workspace_root::~filesystem_workspace_root() {
  if (!impl_) return;
#if defined(_WIN32)
  if (impl_->handle != INVALID_HANDLE_VALUE && impl_->handle != nullptr) CloseHandle(impl_->handle);
  if (impl_->write_handle != INVALID_HANDLE_VALUE && impl_->write_handle != nullptr) {
    CloseHandle(impl_->write_handle);
  }
#elif defined(__linux__)
  close_uv_descriptor(impl_->descriptor);
#endif
}

filesystem_workspace_root::filesystem_workspace_root(filesystem_workspace_root&&) noexcept = default;
filesystem_workspace_root& filesystem_workspace_root::operator=(filesystem_workspace_root&&) noexcept = default;

bool filesystem_workspace_root::valid() const {
#if defined(_WIN32)
  return impl_ && impl_->handle != INVALID_HANDLE_VALUE && impl_->handle != nullptr;
#elif defined(__linux__)
  return impl_ && impl_->descriptor >= 0;
#else
  return false;
#endif
}

filesystem_capability_port::filesystem_capability_port(
    std::function<void()> before_replace_validation)
    : before_replace_validation_(std::move(before_replace_validation)) {}

filesystem_capability_file::filesystem_capability_file() = default;
filesystem_capability_file::filesystem_capability_file(const int descriptor) : descriptor_(descriptor) {}
filesystem_capability_file::~filesystem_capability_file() { close_uv_descriptor(descriptor_); }
filesystem_capability_file::filesystem_capability_file(filesystem_capability_file&& other) noexcept
    : descriptor_(std::exchange(other.descriptor_, -1)) {}
filesystem_capability_file& filesystem_capability_file::operator=(filesystem_capability_file&& other) noexcept {
  if (this == &other) return *this;
  close_uv_descriptor(descriptor_);
  descriptor_ = std::exchange(other.descriptor_, -1);
  return *this;
}
bool filesystem_capability_file::valid() const { return descriptor_ >= 0; }
int filesystem_capability_file::descriptor() const { return descriptor_; }

filesystem_root_open_result filesystem_capability_port::open_root(
    const std::filesystem::path& selected_path) const {
  filesystem_root_open_result result;
#if defined(_WIN32)
  const auto selected_root_state = selected_windows_root_state(selected_path);
  if (selected_root_state == windows_remote_state::remote) {
    result.error_code = "NETWORK_FILESYSTEM_UNSUPPORTED";
    return result;
  }
  if (selected_root_state == windows_remote_state::unsupported) {
    result.error_code = "PLATFORM_UNSUPPORTED";
    return result;
  }
  std::wstring selected;
  try {
    selected = extended_windows_path(selected_path);
  } catch (...) {
    result.error_code = "INVALID_REQUEST";
    return result;
  }
  const auto selected_attributes = GetFileAttributesW(selected.c_str());
  if (selected_attributes == INVALID_FILE_ATTRIBUTES) {
    result.error_code = windows_error_code(GetLastError());
    return result;
  }
  if ((selected_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U) {
    result.error_code = "NOT_DIRECTORY";
    return result;
  }
  windows_handle_guard handle(CreateFileW(
      selected.c_str(),
      FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS,
      nullptr));
  if (handle.get() == INVALID_HANDLE_VALUE) {
    result.error_code = windows_error_code(GetLastError());
    return result;
  }
  bool is_reparse = false;
  std::uint64_t volume = 0U;
  if (!windows_handle_is_reparse_point(handle.get(), is_reparse)) {
    result.error_code = "PLATFORM_UNSUPPORTED";
    return result;
  }
  if (is_reparse) {
    result.error_code = "PLATFORM_UNSUPPORTED";
    return result;
  }
  if (!query_windows_identity(handle.get(), result.identity, volume)) {
    result.error_code = "PLATFORM_UNSUPPORTED";
    return result;
  }
  const auto remote_state = query_windows_remote_state(handle.get());
  if (remote_state == windows_remote_state::remote) {
    result.error_code = "NETWORK_FILESYSTEM_UNSUPPORTED";
    return result;
  }
  windows_handle_guard write_handle(CreateFileW(
      selected.c_str(),
      FILE_LIST_DIRECTORY | FILE_ADD_FILE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS,
      nullptr));
  if (write_handle.get() != INVALID_HANDLE_VALUE) {
    std::string write_identity;
    std::uint64_t write_volume = 0U;
    bool write_reparse = false;
    if (!windows_handle_is_reparse_point(write_handle.get(), write_reparse)
        || write_reparse
        || !query_windows_identity(write_handle.get(), write_identity, write_volume)
        || write_identity != result.identity
        || write_volume != volume) {
      write_handle = windows_handle_guard();
    }
  }
  result.root.impl_ = std::make_unique<filesystem_workspace_root::impl>();
  result.root.impl_->handle = handle.release();
  result.root.impl_->write_handle = write_handle.release();
  result.root.impl_->volume = volume;
  result.root.impl_->identity = result.identity;
  result.ok = true;
#elif defined(__linux__)
  std::string encoded;
  try {
    encoded = path_bytes(selected_path);
  } catch (...) {
    result.error_code = "INVALID_REQUEST";
    return result;
  }
  const auto descriptor = open(encoded.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
  if (descriptor < 0) {
    result.error_code = linux_error_code(errno);
    return result;
  }
  struct stat status{};
  if (fstat(descriptor, &status) != 0 || !S_ISDIR(status.st_mode)) {
    close_uv_descriptor(descriptor);
    result.error_code = "NOT_DIRECTORY";
    return result;
  }
  if (!linux_openat2_supported(descriptor)) {
    close_uv_descriptor(descriptor);
    result.error_code = "PLATFORM_UNSUPPORTED";
    return result;
  }
  result.identity = linux_identity(status);
  result.root.impl_ = std::make_unique<filesystem_workspace_root::impl>();
  result.root.impl_->descriptor = descriptor;
  result.root.impl_->identity = result.identity;
  result.ok = true;
#else
  static_cast<void>(selected_path);
  result.error_code = "PLATFORM_UNSUPPORTED";
#endif
  return result;
}

filesystem_directory_result filesystem_capability_port::list_directory(
    const filesystem_workspace_root& root,
    const filesystem_component_chain& components,
    const std::string_view expected_identity,
    const std::size_t maximum_entries,
    const std::size_t maximum_metadata_bytes) const {
  filesystem_directory_result result;
  if (!root.valid() || !safe_component_chain(components) || expected_identity.empty()) {
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }

#if defined(_WIN32)
  HANDLE directory_handle = root.impl_->handle;
  windows_handle_guard opened_directory;
  if (components.empty()) {
    std::string current_identity;
    std::uint64_t current_volume = 0U;
    if (!query_windows_identity(directory_handle, current_identity, current_volume)
        || current_identity != expected_identity
        || current_volume != root.impl_->volume) {
      result.error_code = "WORKSPACE_INVALID";
      return result;
    }
  } else {
    auto opened = open_windows_relative(
        root.impl_->handle,
        root.impl_->volume,
        components,
        filesystem_capability_entry_kind::directory,
        expected_identity);
    if (!opened.ok) {
      result.error_code = opened.error_code;
      return result;
    }
    opened_directory = std::move(opened.handle);
    directory_handle = opened_directory.get();
  }

  alignas(std::max_align_t) std::array<unsigned char, k_directory_buffer_bytes> buffer{};
  bool restart = true;
  std::size_t metadata_bytes = 0U;
  while (true) {
    const auto info_class = restart ? FileIdExtdDirectoryRestartInfo : FileIdExtdDirectoryInfo;
    if (GetFileInformationByHandleEx(
            directory_handle,
            info_class,
            buffer.data(),
            static_cast<DWORD>(buffer.size())) == FALSE) {
      const auto error = GetLastError();
      if (error == ERROR_NO_MORE_FILES) break;
      result.error_code = windows_error_code(error);
      return result;
    }
    restart = false;
    std::size_t offset = 0U;
    while (true) {
      constexpr auto header_bytes = offsetof(FILE_ID_EXTD_DIR_INFO, FileName);
      if (offset > buffer.size() - header_bytes) {
        result.error_code = "WORKSPACE_INVALID";
        return result;
      }
      const auto* info = reinterpret_cast<const FILE_ID_EXTD_DIR_INFO*>(buffer.data() + offset);
      if ((info->FileNameLength % sizeof(wchar_t)) != 0U
          || info->FileNameLength > buffer.size() - offset - header_bytes) {
        result.error_code = "WORKSPACE_INVALID";
        return result;
      }
      const auto wide_name = std::wstring_view(
          info->FileName,
          static_cast<std::size_t>(info->FileNameLength / sizeof(wchar_t)));
      std::string name;
      const auto name_valid_utf8 = wide_to_utf8(wide_name, name);
      if (name_valid_utf8 && name != "." && name != "..") {
        if (result.entries.size() >= maximum_entries) {
          result.error_code = "DIRECTORY_RESOURCE_LIMIT";
          return result;
        }
        filesystem_capability_entry entry;
        entry.display_name = escaped_display_name(name);
        entry.kind = windows_entry_kind(info->FileAttributes);
        const auto component_valid = is_safe_native_capability_child_name(name);
        if (component_valid) entry.name = name;
        metadata_bytes += entry.display_name.size() + k_entry_metadata_overhead;
        if (metadata_bytes > maximum_metadata_bytes) {
          result.error_code = "DIRECTORY_RESOURCE_LIMIT";
          return result;
        }
        if (entry.kind == filesystem_capability_entry_kind::regular_file
            && info->EndOfFile.QuadPart >= 0) {
          entry.byte_size = static_cast<std::uint64_t>(info->EndOfFile.QuadPart);
        }
        if (component_valid
            && (entry.kind == filesystem_capability_entry_kind::directory
                || entry.kind == filesystem_capability_entry_kind::regular_file)) {
          if (file_id_is_zero(info->FileId)) {
            result.error_code = "PLATFORM_UNSUPPORTED";
            return result;
          }
          entry.identity = windows_identity(root.impl_->volume, info->FileId);
          entry.accessible = true;
        }
        result.entries.push_back(std::move(entry));
      }
      if (info->NextEntryOffset == 0U) break;
      if (info->NextEntryOffset < header_bytes || info->NextEntryOffset > buffer.size() - offset) {
        result.error_code = "WORKSPACE_INVALID";
        return result;
      }
      offset += info->NextEntryOffset;
    }
  }

  if (!components.empty()) {
    const auto verified = open_windows_relative(
        root.impl_->handle,
        root.impl_->volume,
        components,
        filesystem_capability_entry_kind::directory,
        expected_identity);
    if (!verified.ok) {
      result.error_code = verified.error_code;
      result.entries.clear();
      return result;
    }
  }
  result.ok = true;
#elif defined(__linux__)
  int directory_descriptor = -1;
  if (components.empty()) {
    directory_descriptor = dup(root.impl_->descriptor);
    if (directory_descriptor < 0) {
      result.error_code = linux_error_code(errno);
      return result;
    }
    struct stat current{};
    if (fstat(directory_descriptor, &current) != 0 || linux_identity(current) != expected_identity) {
      close_uv_descriptor(directory_descriptor);
      result.error_code = "WORKSPACE_INVALID";
      return result;
    }
  } else {
    auto opened = open_linux_relative(
        root.impl_->descriptor,
        components,
        filesystem_capability_entry_kind::directory,
        expected_identity);
    if (!opened.ok) {
      result.error_code = opened.error_code;
      return result;
    }
    directory_descriptor = opened.descriptor;
  }
  DIR* stream = fdopendir(directory_descriptor);
  if (stream == nullptr) {
    close_uv_descriptor(directory_descriptor);
    result.error_code = linux_error_code(errno);
    return result;
  }

  std::size_t metadata_bytes = 0U;
  errno = 0;
  while (const auto* item = readdir(stream)) {
    const std::string raw_name(item->d_name);
    if (raw_name == "." || raw_name == "..") continue;
    if (result.entries.size() >= maximum_entries) {
      closedir(stream);
      result.error_code = "DIRECTORY_RESOURCE_LIMIT";
      return result;
    }
    filesystem_capability_entry entry;
    entry.display_name = escaped_display_name(raw_name);
    const auto byte_span = std::span(
        reinterpret_cast<const unsigned char*>(raw_name.data()), raw_name.size());
    const auto component_valid = valid_utf8(byte_span) && is_safe_native_capability_child_name(raw_name);
    if (component_valid) entry.name = raw_name;
    metadata_bytes += entry.display_name.size() + k_entry_metadata_overhead;
    if (metadata_bytes > maximum_metadata_bytes) {
      closedir(stream);
      result.error_code = "DIRECTORY_RESOURCE_LIMIT";
      return result;
    }

    struct stat no_follow{};
    if (fstatat(directory_descriptor, raw_name.c_str(), &no_follow, AT_SYMLINK_NOFOLLOW) == 0) {
      if (S_ISLNK(no_follow.st_mode)) {
        entry.kind = filesystem_capability_entry_kind::symbolic_link;
      } else if (S_ISDIR(no_follow.st_mode) || S_ISREG(no_follow.st_mode)) {
        entry.kind = S_ISDIR(no_follow.st_mode)
            ? filesystem_capability_entry_kind::directory
            : filesystem_capability_entry_kind::regular_file;
        if (S_ISREG(no_follow.st_mode) && no_follow.st_size >= 0) {
          entry.byte_size = static_cast<std::uint64_t>(no_follow.st_size);
        }
        if (component_valid) {
          auto child_components = components;
          child_components.push_back(raw_name);
          auto opened = open_linux_relative(
              root.impl_->descriptor,
              child_components,
              entry.kind,
              linux_identity(no_follow));
          if (opened.ok) {
            entry.identity = opened.identity;
            entry.accessible = true;
            close_uv_descriptor(opened.descriptor);
          }
        }
      }
    }
    result.entries.push_back(std::move(entry));
    errno = 0;
  }
  const auto enumeration_error = errno;
  closedir(stream);
  if (enumeration_error != 0) {
    result.error_code = linux_error_code(enumeration_error);
    return result;
  }
  if (!components.empty()) {
    const auto verified = open_linux_relative(
        root.impl_->descriptor,
        components,
        filesystem_capability_entry_kind::directory,
        expected_identity);
    if (!verified.ok) {
      result.error_code = verified.error_code;
      result.entries.clear();
      return result;
    }
    close_uv_descriptor(verified.descriptor);
  }
  result.ok = true;
#else
  static_cast<void>(components);
  static_cast<void>(maximum_entries);
  static_cast<void>(maximum_metadata_bytes);
  result.error_code = "PLATFORM_UNSUPPORTED";
#endif
  return result;
}

filesystem_file_open_result filesystem_capability_port::open_regular_file(
    const filesystem_workspace_root& root,
    const filesystem_component_chain& components,
    const std::string_view expected_identity) const {
  filesystem_file_open_result result;
  if (!root.valid() || components.empty() || !safe_component_chain(components)
      || expected_identity.empty()) {
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }
#if defined(_WIN32)
  auto opened = open_windows_relative(
      root.impl_->handle,
      root.impl_->volume,
      components,
      filesystem_capability_entry_kind::regular_file,
      expected_identity);
  if (!opened.ok) {
    result.error_code = opened.error_code;
    return result;
  }
  const auto descriptor = _open_osfhandle(
      reinterpret_cast<std::intptr_t>(opened.handle.get()),
      _O_RDONLY | _O_BINARY);
  if (descriptor < 0) {
    result.error_code = "INTERNAL_ERROR";
    return result;
  }
  static_cast<void>(opened.handle.release());
  result.file = filesystem_capability_file(descriptor);
  result.identity = opened.identity;
  result.ok = true;
#elif defined(__linux__)
  auto opened = open_linux_relative(
      root.impl_->descriptor,
      components,
      filesystem_capability_entry_kind::regular_file,
      expected_identity);
  if (!opened.ok) {
    result.error_code = opened.error_code;
    return result;
  }
  result.file = filesystem_capability_file(opened.descriptor);
  result.identity = opened.identity;
  result.ok = true;
#else
  static_cast<void>(components);
  result.error_code = "PLATFORM_UNSUPPORTED";
#endif
  return result;
}

filesystem_file_open_result filesystem_capability_port::authorize_regular_file(
    const filesystem_workspace_root& root,
    const filesystem_component_chain& components) const {
  filesystem_file_open_result result;
  if (!root.valid() || components.empty() || !safe_component_chain(components)) {
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }
#if defined(_WIN32)
  auto opened = open_windows_relative(
      root.impl_->handle,
      root.impl_->volume,
      components,
      filesystem_capability_entry_kind::regular_file,
      "");
  if (!opened.ok) {
    result.error_code = opened.error_code;
    return result;
  }
  const auto descriptor = _open_osfhandle(
      reinterpret_cast<std::intptr_t>(opened.handle.get()),
      _O_RDONLY | _O_BINARY);
  if (descriptor < 0) {
    result.error_code = "INTERNAL_ERROR";
    return result;
  }
  static_cast<void>(opened.handle.release());
  result.file = filesystem_capability_file(descriptor);
  result.identity = opened.identity;
  result.ok = true;
#elif defined(__linux__)
  auto opened = open_linux_relative(
      root.impl_->descriptor,
      components,
      filesystem_capability_entry_kind::regular_file,
      "");
  if (!opened.ok) {
    result.error_code = opened.error_code;
    return result;
  }
  result.file = filesystem_capability_file(opened.descriptor);
  result.identity = opened.identity;
  result.ok = true;
#else
  static_cast<void>(components);
  result.error_code = "PLATFORM_UNSUPPORTED";
#endif
  return result;
}

bool filesystem_capability_port::verify_regular_file(
    const filesystem_workspace_root& root,
    const filesystem_component_chain& components,
    const std::string_view expected_identity) const {
  auto opened = open_regular_file(root, components, expected_identity);
  return opened.ok;
}

filesystem_replace_result filesystem_capability_port::replace_regular_file(
    const filesystem_workspace_root& root,
    const filesystem_component_chain& components,
    const std::string_view expected_identity,
    const std::string_view expected_content_hash,
    const std::span<const unsigned char> replacement_bytes) const {
  filesystem_replace_result result;
  if (!root.valid() || components.empty() || !safe_component_chain(components)
      || expected_identity.empty() || expected_content_hash.size() != 64U
      || replacement_bytes.size() > k_maximum_capability_file_bytes) {
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }

#if defined(_WIN32)
  auto parent_components = components;
  const auto target_component = parent_components.back();
  parent_components.pop_back();
  std::wstring target_name;
  if (!utf8_to_wide(target_component, target_name)) {
    result.error_code = "WORKSPACE_INVALID";
    return result;
  }

  windows_handle_guard parent_handle;
  if (parent_components.empty()) {
    if (root.impl_->write_handle == INVALID_HANDLE_VALUE || root.impl_->write_handle == nullptr) {
      result.error_code = "PERMISSION_DENIED";
      return result;
    }
    HANDLE duplicate = INVALID_HANDLE_VALUE;
    if (DuplicateHandle(
            GetCurrentProcess(),
            root.impl_->write_handle,
            GetCurrentProcess(),
            &duplicate,
            0U,
            FALSE,
            DUPLICATE_SAME_ACCESS) == FALSE) {
      result.error_code = windows_save_error_code(GetLastError());
      return result;
    }
    parent_handle = windows_handle_guard(duplicate);
  } else {
    auto opened_parent = open_windows_relative(
        root.impl_->handle,
        root.impl_->volume,
        parent_components,
        filesystem_capability_entry_kind::directory,
        "",
        FILE_ADD_FILE);
    if (!opened_parent.ok) {
      result.error_code = opened_parent.error_code;
      return result;
    }
    parent_handle = std::move(opened_parent.handle);
  }

  std::wstring temporary_name;
  std::string operation_error;
  auto temporary = create_windows_temporary_file(
      parent_handle.get(), temporary_name, operation_error);
  if (temporary.get() == INVALID_HANDLE_VALUE) {
    result.error_code = operation_error;
    return result;
  }
  bool renamed = false;
  const auto cleanup_temporary = [&]() {
    if (!renamed) mark_windows_file_for_delete(temporary.get());
  };
  if (!write_windows_file(temporary.get(), replacement_bytes, operation_error)) {
    cleanup_temporary();
    result.error_code = operation_error;
    return result;
  }

  auto target = open_windows_relative(
      root.impl_->handle,
      root.impl_->volume,
      components,
      filesystem_capability_entry_kind::regular_file,
      expected_identity,
      READ_CONTROL);
  if (!target.ok) {
    cleanup_temporary();
    result.error_code = target.error_code;
    return result;
  }
  std::string current_hash;
  std::uint64_t link_count = 0U;
  if (!windows_regular_file_hash(
          target.handle.get(), current_hash, link_count, operation_error)) {
    cleanup_temporary();
    result.error_code = operation_error;
    return result;
  }
  if (current_hash != expected_content_hash) {
    cleanup_temporary();
    result.error_code = "DOCUMENT_CONFLICT";
    return result;
  }
  if (link_count != 1U) {
    cleanup_temporary();
    result.error_code = "UNSAFE_FILE_METADATA";
    return result;
  }
  if (!copy_windows_metadata(target.handle.get(), temporary.get(), operation_error)
      || FlushFileBuffers(temporary.get()) == FALSE) {
    cleanup_temporary();
    result.error_code = operation_error.empty()
        ? windows_save_error_code(GetLastError())
        : operation_error;
    return result;
  }
  if (before_replace_validation_) before_replace_validation_();
  auto final_target = open_windows_relative(
      root.impl_->handle,
      root.impl_->volume,
      components,
      filesystem_capability_entry_kind::regular_file,
      expected_identity,
      READ_CONTROL);
  std::string final_hash;
  std::uint64_t final_link_count = 0U;
  if (!final_target.ok
      || !windows_regular_file_hash(
          final_target.handle.get(), final_hash, final_link_count, operation_error)
      || final_hash != expected_content_hash || final_link_count != 1U) {
    cleanup_temporary();
    result.error_code = final_target.ok && operation_error.empty()
        ? "DOCUMENT_CONFLICT"
        : final_target.ok ? operation_error : final_target.error_code;
    return result;
  }
  final_target.handle = windows_handle_guard();
  target.handle = windows_handle_guard();

  const auto rename_bytes = offsetof(FILE_RENAME_INFORMATION, FileName)
      + target_name.size() * sizeof(wchar_t);
  std::vector<unsigned char> rename_storage(rename_bytes);
  auto* rename_info = reinterpret_cast<FILE_RENAME_INFORMATION*>(rename_storage.data());
  *reinterpret_cast<ULONG*>(rename_storage.data()) = 0x00000001U | 0x00000002U;
  rename_info->RootDirectory = parent_handle.get();
  rename_info->FileNameLength = static_cast<ULONG>(target_name.size() * sizeof(wchar_t));
  std::memcpy(rename_info->FileName, target_name.data(), rename_info->FileNameLength);
  IO_STATUS_BLOCK rename_status_block{};
  FILE_INFORMATION_CLASS rename_information_class{};
  static_assert(sizeof(rename_information_class) == sizeof(ULONG));
  const ULONG rename_information_value = 65U;
  std::memcpy(
      &rename_information_class,
      &rename_information_value,
      sizeof(rename_information_class));
  const auto rename_status = NtSetInformationFile(
      temporary.get(),
      &rename_status_block,
      rename_info,
      static_cast<ULONG>(rename_storage.size()),
      rename_information_class);
  if (rename_status < 0) {
    cleanup_temporary();
    const auto rename_error = RtlNtStatusToDosError(rename_status);
    result.error_code = windows_save_error_code(rename_error);
    return result;
  }
  renamed = true;
  std::uint64_t new_volume = 0U;
  if (!query_windows_identity(temporary.get(), result.identity, new_volume)
      || new_volume != root.impl_->volume) {
    result.error_code = "SAVE_OUTCOME_UNKNOWN";
    return result;
  }
  result.ok = true;

#elif defined(__linux__)
  auto parent_components = components;
  const auto target_name = parent_components.back();
  parent_components.pop_back();
  int parent_descriptor = -1;
  if (parent_components.empty()) {
    parent_descriptor = dup(root.impl_->descriptor);
    if (parent_descriptor < 0) {
      result.error_code = linux_save_error_code(errno);
      return result;
    }
  } else {
    auto opened_parent = open_linux_relative(
        root.impl_->descriptor,
        parent_components,
        filesystem_capability_entry_kind::directory,
        "");
    if (!opened_parent.ok) {
      result.error_code = opened_parent.error_code;
      return result;
    }
    parent_descriptor = opened_parent.descriptor;
  }

  std::string temporary_name;
  int temporary_descriptor = -1;
  for (int attempt = 0; attempt < 8; ++attempt) {
    temporary_name = ".loop-save-" + loop::support::secure_random_hex(16U) + ".tmp";
    temporary_descriptor = openat(
        parent_descriptor,
        temporary_name.c_str(),
        O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        0600);
    if (temporary_descriptor >= 0) break;
    if (errno != EEXIST) {
      result.error_code = linux_save_error_code(errno);
      close_uv_descriptor(parent_descriptor);
      return result;
    }
  }
  if (temporary_descriptor < 0) {
    result.error_code = "INTERNAL_ERROR";
    close_uv_descriptor(parent_descriptor);
    return result;
  }
  bool renamed = false;
  const auto cleanup = [&]() {
    if (!renamed) {
      static_cast<void>(unlinkat(parent_descriptor, temporary_name.c_str(), 0));
    }
    close_uv_descriptor(temporary_descriptor);
    close_uv_descriptor(parent_descriptor);
  };
  std::string operation_error;
  if (!write_linux_file(temporary_descriptor, replacement_bytes, operation_error)) {
    result.error_code = operation_error;
    cleanup();
    return result;
  }

  auto target = open_linux_relative(
      root.impl_->descriptor,
      components,
      filesystem_capability_entry_kind::regular_file,
      expected_identity);
  if (!target.ok) {
    result.error_code = target.error_code;
    cleanup();
    return result;
  }
  std::string current_hash;
  std::uint64_t link_count = 0U;
  if (!linux_regular_file_hash(
          target.descriptor, current_hash, link_count, operation_error)) {
    result.error_code = operation_error;
    close_uv_descriptor(target.descriptor);
    cleanup();
    return result;
  }
  if (current_hash != expected_content_hash) {
    result.error_code = "DOCUMENT_CONFLICT";
    close_uv_descriptor(target.descriptor);
    cleanup();
    return result;
  }
  if (link_count != 1U || !copy_linux_metadata(target.descriptor, temporary_descriptor)) {
    result.error_code = "UNSAFE_FILE_METADATA";
    close_uv_descriptor(target.descriptor);
    cleanup();
    return result;
  }
  if (before_replace_validation_) before_replace_validation_();
  auto final_target = open_linux_relative(
      root.impl_->descriptor,
      components,
      filesystem_capability_entry_kind::regular_file,
      expected_identity);
  std::string final_hash;
  std::uint64_t final_link_count = 0U;
  if (!final_target.ok
      || !linux_regular_file_hash(
          final_target.descriptor, final_hash, final_link_count, operation_error)
      || final_hash != expected_content_hash || final_link_count != 1U) {
    result.error_code = final_target.ok && operation_error.empty()
        ? "DOCUMENT_CONFLICT"
        : final_target.ok ? operation_error : final_target.error_code;
    if (final_target.descriptor >= 0) close_uv_descriptor(final_target.descriptor);
    close_uv_descriptor(target.descriptor);
    cleanup();
    return result;
  }
  close_uv_descriptor(final_target.descriptor);
  close_uv_descriptor(target.descriptor);
  if (fsync(temporary_descriptor) != 0) {
    result.error_code = linux_save_error_code(errno);
    cleanup();
    return result;
  }
  if (renameat(
          parent_descriptor,
          temporary_name.c_str(),
          parent_descriptor,
          target_name.c_str()) != 0) {
    result.error_code = linux_save_error_code(errno);
    cleanup();
    return result;
  }
  renamed = true;
  if (fsync(parent_descriptor) != 0) {
    result.error_code = "SAVE_OUTCOME_UNKNOWN";
    cleanup();
    return result;
  }
  struct stat final_status{};
  if (fstat(temporary_descriptor, &final_status) != 0) {
    result.error_code = "SAVE_OUTCOME_UNKNOWN";
    cleanup();
    return result;
  }
  result.identity = linux_identity(final_status);
  result.ok = true;
  cleanup();
#else
  static_cast<void>(replacement_bytes);
  result.error_code = "PLATFORM_UNSUPPORTED";
#endif
  return result;
}

}  // namespace loop::service
