#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "service/workspace_service.h"
#include "service/filesystem_capability_port.h"
#include "support/portable_crypto.h"

#include <uv.h>

namespace {

namespace fs = std::filesystem;
using loop::service::service_result;
using loop::service::workspace_service;

int failures = 0;

void expect(const bool condition, const std::string& message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

class temporary_directory {
 public:
  temporary_directory() {
    path_ = fs::temp_directory_path() / ("loop-workspace-test-" + loop::support::secure_random_hex(8U));
    fs::create_directories(path_);
  }

  ~temporary_directory() {
    std::error_code ignored;
    fs::remove_all(path_, ignored);
  }

  temporary_directory(const temporary_directory&) = delete;
  temporary_directory& operator=(const temporary_directory&) = delete;
  [[nodiscard]] const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

void write_bytes(const fs::path& path, const std::vector<unsigned char>& bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!output) throw std::runtime_error("test fixture write failed");
}

std::vector<unsigned char> read_bytes(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  const std::string bytes{
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>()};
  return {bytes.begin(), bytes.end()};
}

std::string utf8_path(const fs::path& path) {
  const auto value = path.u8string();
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}

fs::path path_from_utf8_for_test(const std::string_view value) {
  std::u8string converted;
  converted.reserve(value.size());
  for (const char character : value) {
    converted.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
  }
  return fs::path(converted);
}

bool create_directory_link(const fs::path& link, const fs::path& target) {
  const auto link_value = utf8_path(link);
  const auto target_value = utf8_path(target);
  uv_fs_t request{};
  const auto result = uv_fs_symlink(
      nullptr,
      &request,
      target_value.c_str(),
      link_value.c_str(),
      UV_FS_SYMLINK_JUNCTION,
      nullptr);
  uv_fs_req_cleanup(&request);
  return result >= 0;
}

void markdown_inspection_is_strict() {
  loop::service::markdown_inspection inspection;
  const std::vector<unsigned char> bom_crlf{0xEFU, 0xBBU, 0xBFU, '#', ' ', 'A', '\r', '\n'};
  expect(loop::service::inspect_markdown_bytes(bom_crlf, inspection), "UTF-8 BOM markdown is accepted");
  expect(inspection.bom, "BOM is reported");
  expect(inspection.line_ending == "crlf", "CRLF is reported");

  const std::vector<unsigned char> bom_only{0xEFU, 0xBBU, 0xBFU};
  expect(loop::service::inspect_markdown_bytes(bom_only, inspection), "BOM-only Markdown is accepted");
  expect(inspection.bom && inspection.line_ending == "none", "BOM-only metadata is reported");

  const std::vector<unsigned char> mixed{'a', '\r', '\n', 'b', '\n'};
  expect(loop::service::inspect_markdown_bytes(mixed, inspection), "mixed line ending markdown is accepted");
  expect(inspection.line_ending == "mixed", "mixed line endings are reported");
  const std::vector<unsigned char> lf{'a', '\n', 'b', '\n'};
  expect(loop::service::inspect_markdown_bytes(lf, inspection), "LF Markdown is accepted");
  expect(inspection.line_ending == "lf", "LF line endings are reported");

  const std::vector<unsigned char> invalid_utf8{0xC0U, 0xAFU};
  expect(!loop::service::inspect_markdown_bytes(invalid_utf8, inspection), "overlong UTF-8 is rejected");
  const std::vector<unsigned char> nul{'a', 0U, 'b'};
  expect(!loop::service::inspect_markdown_bytes(nul, inspection), "NUL is rejected");
}

void sha256_matches_standard_vector() {
  const std::string input = "abc";
  const auto bytes = std::span(
      reinterpret_cast<const unsigned char*>(input.data()),
      input.size());
  expect(
      loop::support::sha256_hex(bytes)
          == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      "SHA-256 matches the standard abc vector");
}

void capability_child_names_are_single_components() {
  using loop::service::filesystem_capability_platform;
  using loop::service::is_safe_capability_child_name;
  expect(is_safe_capability_child_name("note.md", filesystem_capability_platform::linux),
      "a Linux child name accepts one ordinary component");
  expect(is_safe_capability_child_name("note:part.md", filesystem_capability_platform::linux),
      "a colon remains a normal Linux filename byte");
  expect(!is_safe_capability_child_name("", filesystem_capability_platform::linux),
      "an empty child name is rejected");
  expect(!is_safe_capability_child_name(".", filesystem_capability_platform::linux),
      "the current-directory component is rejected");
  expect(!is_safe_capability_child_name("..", filesystem_capability_platform::linux),
      "the parent-directory component is rejected");
  expect(!is_safe_capability_child_name("a/b", filesystem_capability_platform::linux),
      "a Linux child name cannot contain a separator");
  expect(!is_safe_capability_child_name(std::string_view("a\0b", 3U), filesystem_capability_platform::linux),
      "a child name cannot contain NUL");
  expect(!is_safe_capability_child_name("a\\b", filesystem_capability_platform::windows),
      "a Windows child name cannot contain a reverse separator");
  expect(!is_safe_capability_child_name("note.md:stream", filesystem_capability_platform::windows),
      "a Windows child name cannot select an alternate data stream");
}

void opening_file_validates_content_and_hides_locator() {
  temporary_directory temporary;
  const auto file = temporary.path() / "valid.md";
  write_bytes(file, {'#', ' ', 'T', 'e', 's', 't', '\n'});

  workspace_service service;
  const auto result = service.open_file("window_a", utf8_path(file));
  expect(result.ok, "valid Markdown opens");
  expect(result.value.at("mode") == "single_file", "single file mode is returned");
  expect(result.value.at("document").at("encoding") == "utf-8", "UTF-8 metadata is returned");
  expect(result.value.dump().find(utf8_path(temporary.path())) == std::string::npos,
      "absolute locator is absent from the response");

  const auto invalid = temporary.path() / "invalid.md";
  write_bytes(invalid, {0xFFU, 0xFEU});
  const auto invalid_result = service.open_file("window_a", utf8_path(invalid));
  expect(!invalid_result.ok && invalid_result.error.code == "INVALID_ENCODING", "invalid UTF-8 is rejected");

  const auto wrong_extension = temporary.path() / "note.txt";
  write_bytes(wrong_extension, {'o', 'k'});
  const auto extension_result = service.open_file("window_a", utf8_path(wrong_extension));
  expect(!extension_result.ok && extension_result.error.code == "NOT_REGULAR_FILE",
      "non-Markdown extension is rejected");

  const auto read_only = temporary.path() / "read-only.md";
  write_bytes(read_only, {'o', 'k'});
  std::error_code permission_error;
  fs::permissions(read_only, fs::perms::owner_read, fs::perm_options::replace, permission_error);
  if (!permission_error) {
    const auto read_only_result = service.open_file("window_a", utf8_path(read_only));
    expect(read_only_result.ok && read_only_result.value.at("document").at("read_only").get<bool>(),
        "read-only metadata is reported without attempting a write");
    fs::permissions(read_only, fs::perms::owner_all, fs::perm_options::replace, permission_error);
  }

  const auto exact_limit = temporary.path() / "exact-limit.md";
  write_bytes(exact_limit, std::vector<unsigned char>(loop::service::k_maximum_markdown_bytes, 'x'));
  const auto exact_opened = service.open_file("window_a", utf8_path(exact_limit));
  expect(exact_opened.ok, "a file at the 5 MiB limit opens");
  if (exact_opened.ok) {
    const auto exact_document = service.open_document(
        "window_a",
        exact_opened.value.at("workspace_id").get<std::string>(),
        "document",
        exact_opened.value.at("document").at("document_id").get<std::string>());
    expect(exact_document.ok && exact_document.body.size() == loop::service::k_maximum_markdown_bytes,
        "a file at the 5 MiB limit returns a body attachment");
  }

  const auto directory_result = service.open_file("window_a", utf8_path(temporary.path()));
  expect(!directory_result.ok && directory_result.error.code == "NOT_REGULAR_FILE",
      "a directory cannot enter the document path");

  const auto large = temporary.path() / "large.md";
  write_bytes(large, std::vector<unsigned char>(loop::service::k_maximum_markdown_bytes + 1U, 'x'));
  const auto large_result = service.open_file("window_a", utf8_path(large));
  expect(!large_result.ok && large_result.error.code == "CONTENT_TOO_LARGE", "files over 5 MiB are rejected");
}

void folder_capability_is_window_bound_and_paginated() {
  temporary_directory temporary;
  write_bytes(temporary.path() / "invalid-body.md", {0xFFU});
  for (int index = 0; index < 300; ++index) {
    write_bytes(temporary.path() / ("note-" + std::to_string(index) + ".md"), {});
  }

  workspace_service service;
  const auto opened = service.open_folder("window_a", utf8_path(temporary.path()));
  expect(opened.ok, "folder opens without reading invalid file bodies");
  if (!opened.ok) return;
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto directory_id = opened.value.at("root_directory_id").get<std::string>();

  const auto cross_window = service.list_children("window_b", workspace_id, directory_id, "");
  expect(!cross_window.ok && cross_window.error.code == "WORKSPACE_INVALID", "workspace cannot cross windows");
  const auto forged_workspace = service.list_children(
      "window_a", "workspace_ffffffffffffffffffffffffffffffff", directory_id, "");
  expect(!forged_workspace.ok && forged_workspace.error.code == "WORKSPACE_INVALID",
      "forged workspace ID is rejected");
  const auto forged_directory = service.list_children(
      "window_a", workspace_id, "directory_ffffffffffffffffffffffffffffffff", "");
  expect(!forged_directory.ok && forged_directory.error.code == "WORKSPACE_INVALID",
      "forged directory ID is rejected");

  const auto first_page = service.list_children("window_a", workspace_id, directory_id, "");
  expect(first_page.ok, "first directory page opens");
  expect(first_page.value.at("entries").size() <= loop::service::k_maximum_entries_per_page,
      "first page respects item limit");
  expect(first_page.value.at("total_entries") == 301, "all first-level metadata is counted");
  expect(!first_page.value.at("next_cursor").is_null(), "continuation cursor is returned");
  expect(first_page.value.dump().find(utf8_path(temporary.path())) == std::string::npos,
      "directory response contains no absolute path");

  const auto cursor = first_page.value.at("next_cursor").get<std::string>();
  const auto second_page = service.list_children("window_a", workspace_id, directory_id, cursor);
  expect(second_page.ok, "second directory page opens");
  expect(second_page.value.at("next_cursor").is_null(), "final page has no cursor");

  const auto replay = service.list_children("window_a", workspace_id, directory_id, cursor);
  expect(!replay.ok && replay.error.code == "WORKSPACE_INVALID", "consumed cursor cannot be replayed");

  const auto fresh_page = service.list_children("window_a", workspace_id, directory_id, "");
  const auto cross_workspace_cursor = fresh_page.value.at("next_cursor").get<std::string>();
  temporary_directory replacement;
  const auto replacement_opened = service.open_folder("window_a", utf8_path(replacement.path()));
  const auto cursor_from_old_workspace = service.list_children(
      "window_a",
      replacement_opened.value.at("workspace_id").get<std::string>(),
      replacement_opened.value.at("root_directory_id").get<std::string>(),
      cross_workspace_cursor);
  expect(!cursor_from_old_workspace.ok && cursor_from_old_workspace.error.code == "WORKSPACE_INVALID",
      "cursor cannot cross workspace replacement");
  expect(service.close("window_a").ok, "workspace closes");
  const auto after_close = service.list_children("window_a", workspace_id, directory_id, "");
  expect(!after_close.ok && after_close.error.code == "WORKSPACE_INVALID", "closed capability is revoked");
}

const nlohmann::json* find_entry(const nlohmann::json& page, const std::string_view name) {
  for (const auto& entry : page.at("entries")) {
    if (entry.at("name") == name) return &entry;
  }
  return nullptr;
}

void document_reads_are_handle_bound_and_capability_scoped() {
  temporary_directory temporary;
  const auto bom_file = temporary.path() / "bom.md";
  const auto empty_file = temporary.path() / "empty.md";
  const auto invalid_file = temporary.path() / "invalid.md";
  const auto nul_file = temporary.path() / "nul.md";
  write_bytes(bom_file, {0xEFU, 0xBBU, 0xBFU, 'a', '\r', '\n'});
  write_bytes(empty_file, {});
  write_bytes(invalid_file, {0xC0U, 0xAFU});
  write_bytes(nul_file, {'a', 0U, 'b'});

  workspace_service service;
  const auto opened = service.open_folder("window_a", utf8_path(temporary.path()));
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto directory_id = opened.value.at("root_directory_id").get<std::string>();
  const auto page = service.list_children("window_a", workspace_id, directory_id, "");
  expect(page.ok, "document fixture folder enumerates");
  const auto* bom_entry = find_entry(page.value, "bom.md");
  const auto* empty_entry = find_entry(page.value, "empty.md");
  const auto* invalid_entry = find_entry(page.value, "invalid.md");
  const auto* nul_entry = find_entry(page.value, "nul.md");
  expect(bom_entry != nullptr && empty_entry != nullptr && invalid_entry != nullptr && nul_entry != nullptr,
      "document fixtures have entry capabilities");
  if (bom_entry == nullptr || empty_entry == nullptr || invalid_entry == nullptr || nul_entry == nullptr) return;

  const auto bom_document = service.open_document(
      "window_a", workspace_id, "entry", bom_entry->at("entry_id").get<std::string>());
  expect(bom_document.ok && bom_document.body_present, "folder Markdown opens as body attachment");
  if (!bom_document.ok) {
    std::cerr << "folder document error: " << bom_document.error.code << ' '
              << bom_document.error.user_message << '\n';
    return;
  }
  expect(bom_document.value.at("bom").get<bool>(), "BOM metadata is retained");
  expect(bom_document.value.at("line_ending") == "crlf", "CRLF metadata is retained");
  expect(bom_document.value.at("byte_size") == 6U, "byte size describes raw bytes including BOM");
  expect(std::string(bom_document.body.begin(), bom_document.body.end()) == "a\r\n",
      "editor body excludes UTF-8 BOM");
  expect(bom_document.value.at("content_hash")
      == loop::support::sha256_hex(std::vector<unsigned char>{0xEFU, 0xBBU, 0xBFU, 'a', '\r', '\n'}),
      "content hash covers original bytes");
  const auto document_id = bom_document.value.at("document_id").get<std::string>();

  const auto duplicate = service.open_document(
      "window_a", workspace_id, "entry", bom_entry->at("entry_id").get<std::string>());
  expect(duplicate.ok && duplicate.value.at("document_id") == document_id,
      "same file identity reuses document capability");
  expect(duplicate.ok && duplicate.value.at("file_version_token") != bom_document.value.at("file_version_token"),
      "each successful read issues a fresh file version token");
  const auto cross_window = service.open_document("window_b", workspace_id, "document", document_id);
  expect(!cross_window.ok && cross_window.error.code == "WORKSPACE_INVALID",
      "document capability cannot cross windows");
  const auto forged = service.open_document(
      "window_a", workspace_id, "document", "document_ffffffffffffffffffffffffffffffff");
  expect(!forged.ok && forged.error.code == "WORKSPACE_INVALID", "forged document ID is rejected");

  const auto empty_document = service.open_document(
      "window_a", workspace_id, "entry", empty_entry->at("entry_id").get<std::string>());
  expect(empty_document.ok && empty_document.body_present && empty_document.body.empty(),
      "empty Markdown is an explicitly present empty body");
  const auto invalid_document = service.open_document(
      "window_a", workspace_id, "entry", invalid_entry->at("entry_id").get<std::string>());
  expect(!invalid_document.ok && invalid_document.error.code == "INVALID_ENCODING",
      "invalid UTF-8 body is rejected at document open");
  const auto nul_document = service.open_document(
      "window_a", workspace_id, "entry", nul_entry->at("entry_id").get<std::string>());
  expect(!nul_document.ok && nul_document.error.code == "INVALID_ENCODING",
      "NUL body is rejected at document open");

  const auto refreshed = service.list_children("window_a", workspace_id, directory_id, "");
  expect(refreshed.ok, "directory refresh succeeds");
  const auto stale_entry = service.open_document(
      "window_a", workspace_id, "entry", bom_entry->at("entry_id").get<std::string>());
  expect(!stale_entry.ok && stale_entry.error.code == "WORKSPACE_INVALID",
      "directory refresh revokes old entry ID");
  const auto retained_document = service.open_document("window_a", workspace_id, "document", document_id);
  expect(retained_document.ok, "directory refresh keeps an opened document capability");

  expect(service.close_document("window_a", workspace_id, document_id).ok,
      "closing a document revokes its capability");
  const auto after_document_close = service.open_document(
      "window_a", workspace_id, "document", document_id);
  expect(!after_document_close.ok && after_document_close.error.code == "WORKSPACE_INVALID",
      "closed document ID cannot be reused");
  const auto* refreshed_bom_entry = find_entry(refreshed.value, "bom.md");
  expect(refreshed_bom_entry != nullptr, "refreshed entry remains available after document close");
  if (refreshed_bom_entry != nullptr) {
    const auto reopened = service.open_document(
        "window_a", workspace_id, "entry",
        refreshed_bom_entry->at("entry_id").get<std::string>());
    expect(reopened.ok && reopened.value.at("document_id") != document_id,
        "reopening a closed folder entry issues a fresh document capability");
  }

  expect(service.close("window_a").ok, "document workspace closes");
  const auto after_close = service.open_document("window_a", workspace_id, "document", document_id);
  expect(!after_close.ok && after_close.error.code == "WORKSPACE_INVALID",
      "workspace close revokes document capability");
}

void document_reads_reject_path_mutation_after_handle_read() {
  {
    temporary_directory temporary;
    const auto original = temporary.path() / "rename.md";
    const auto moved = temporary.path() / "renamed.md";
    write_bytes(original, {'o', 'l', 'd'});
    bool renamed = false;
    loop::service::file_service reader([&]() {
      std::error_code rename_error;
      fs::rename(original, moved, rename_error);
      renamed = !rename_error;
    });
    const auto result = reader.read_markdown(original, "");
    if (renamed) {
      expect(!result.ok && (result.error_code == "NOT_FOUND" || result.error_code == "WORKSPACE_INVALID"),
          "path rename after handle read fails closed");
    } else {
      std::cout << "file rename-during-read fixture skipped because the filesystem rejected rename\n";
    }
  }

  {
    temporary_directory temporary;
    const auto original = temporary.path() / "replace.md";
    const auto displaced = temporary.path() / "displaced.md";
    write_bytes(original, {'o', 'l', 'd'});
    bool replaced = false;
    loop::service::file_service reader([&]() {
      std::error_code rename_error;
      fs::rename(original, displaced, rename_error);
      if (rename_error) return;
      write_bytes(original, {'n', 'e', 'w'});
      replaced = true;
    });
    const auto result = reader.read_markdown(original, "");
    if (replaced) {
      expect(!result.ok && result.error_code == "WORKSPACE_INVALID",
          "path identity replacement after handle read fails closed");
    } else {
      std::cout << "file replacement-during-read fixture skipped because the filesystem rejected rename\n";
    }
  }

  {
    temporary_directory temporary;
    const auto original = temporary.path() / "resize.md";
    write_bytes(original, {'o', 'l', 'd'});
    bool resized = false;
    loop::service::file_service reader([&]() {
      std::ofstream output(original, std::ios::binary | std::ios::app);
      output.write("-grown", 6);
      resized = static_cast<bool>(output);
    });
    const auto result = reader.read_markdown(original, "");
    if (resized) {
      expect(!result.ok && result.error_code == "WORKSPACE_INVALID",
          "size mutation after handle read fails closed");
    } else {
      std::cout << "file resize-during-read fixture skipped because the filesystem rejected write\n";
    }
  }
}

void document_save_preserves_format_rotates_tokens_and_detects_conflicts() {
  temporary_directory temporary;
  const auto markdown = temporary.path() / "save.md";
  write_bytes(markdown, {0xEFU, 0xBBU, 0xBFU, 'o', 'l', 'd', '\r', '\n'});

  workspace_service service;
  const auto opened = service.open_file("window_save", utf8_path(markdown));
  expect(opened.ok, "save fixture opens as a single-file capability");
  if (!opened.ok) return;
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto document_id = opened.value.at("document").at("document_id").get<std::string>();
  const auto snapshot = service.open_document("window_save", workspace_id, "document", document_id);
  expect(snapshot.ok, "save fixture obtains a versioned document snapshot");
  if (!snapshot.ok) return;
  const auto old_token = snapshot.value.at("file_version_token").get<std::string>();
  const auto old_hash = snapshot.value.at("content_hash").get<std::string>();
  const std::vector<unsigned char> edited{'n', 'e', 'w', '\n', 'l', 'i', 'n', 'e', '\n'};
  const auto saved = service.save_document(
      "window_save",
      workspace_id,
      document_id,
      old_token,
      old_hash,
      7U,
      "preserve",
      edited);
  expect(saved.ok && saved.value.at("saved_revision") == 7U,
      "a conflict-free save acknowledges the exact editor revision");
  if (!saved.ok) return;
  expect(saved.value.at("file_version_token") != old_token,
      "a successful save rotates the file version token");
  expect(saved.value.at("line_ending") == "crlf" && saved.value.at("bom").get<bool>(),
      "save preserves the CRLF and BOM baseline");
  expect(read_bytes(markdown) == std::vector<unsigned char>({
      0xEFU, 0xBBU, 0xBFU, 'n', 'e', 'w', '\r', '\n', 'l', 'i', 'n', 'e', '\r', '\n'}),
      "save serializes canonical editor LF back to BOM plus CRLF bytes");

  const auto replay = service.save_document(
      "window_save",
      workspace_id,
      document_id,
      old_token,
      old_hash,
      8U,
      "preserve",
      edited);
  expect(!replay.ok && replay.error.code == "DOCUMENT_CONFLICT",
      "a stale save token cannot replay after a successful save");

  write_bytes(markdown, {'e', 'x', 't', 'e', 'r', 'n', 'a', 'l', '\n'});
  const auto external_conflict = service.save_document(
      "window_save",
      workspace_id,
      document_id,
      saved.value.at("file_version_token").get<std::string>(),
      saved.value.at("content_hash").get<std::string>(),
      9U,
      "preserve",
      edited);
  expect(!external_conflict.ok && external_conflict.error.code == "DOCUMENT_CONFLICT",
      "an external in-place change is detected immediately before replacement");
  expect(read_bytes(markdown) == std::vector<unsigned char>({
      'e', 'x', 't', 'e', 'r', 'n', 'a', 'l', '\n'}),
      "a conflict never overwrites the external bytes");
}

void mixed_line_endings_require_an_explicit_normalization_choice() {
  temporary_directory temporary;
  const auto markdown = temporary.path() / "mixed.md";
  write_bytes(markdown, {'a', '\r', '\n', 'b', '\n'});
  workspace_service service;
  const auto opened = service.open_file("window_mixed", utf8_path(markdown));
  if (!opened.ok) {
    expect(false, "mixed-line save fixture opens");
    return;
  }
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto document_id = opened.value.at("document").at("document_id").get<std::string>();
  const auto snapshot = service.open_document("window_mixed", workspace_id, "document", document_id);
  const std::vector<unsigned char> edited{'a', '\n', 'b', '\n', 'c', '\n'};
  const auto preserve = service.save_document(
      "window_mixed",
      workspace_id,
      document_id,
      snapshot.value.at("file_version_token").get<std::string>(),
      snapshot.value.at("content_hash").get<std::string>(),
      1U,
      "preserve",
      edited);
  expect(!preserve.ok && preserve.error.code == "FORMAT_DECISION_REQUIRED",
      "mixed line endings are never normalized silently");
  expect(read_bytes(markdown) == std::vector<unsigned char>({'a', '\r', '\n', 'b', '\n'}),
      "format-decision failure leaves the mixed file byte-for-byte unchanged");

  const auto normalized = service.save_document(
      "window_mixed",
      workspace_id,
      document_id,
      snapshot.value.at("file_version_token").get<std::string>(),
      snapshot.value.at("content_hash").get<std::string>(),
      1U,
      "normalize_lf",
      edited);
  expect(normalized.ok && normalized.value.at("line_ending") == "lf",
      "an explicit LF choice becomes the new disk baseline");
  expect(read_bytes(markdown) == edited, "explicit LF normalization writes only LF separators");
}

void crlf_expansion_cannot_exceed_the_disk_byte_limit() {
  temporary_directory temporary;
  const auto markdown = temporary.path() / "crlf-limit.md";
  const std::vector<unsigned char> original{'a', '\r', '\n'};
  write_bytes(markdown, original);
  workspace_service service;
  const auto opened = service.open_file("window_crlf_limit", utf8_path(markdown));
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto document_id = opened.value.at("document").at("document_id").get<std::string>();
  const auto snapshot = service.open_document(
      "window_crlf_limit", workspace_id, "document", document_id);
  std::vector<unsigned char> canonical(5U * 1024U * 1024U, 'x');
  for (std::size_t index = 1U; index < canonical.size(); index += 2U) canonical[index] = '\n';
  const auto rejected = service.save_document(
      "window_crlf_limit",
      workspace_id,
      document_id,
      snapshot.value.at("file_version_token").get<std::string>(),
      snapshot.value.at("content_hash").get<std::string>(),
      1U,
      "preserve",
      canonical);
  expect(!rejected.ok && rejected.error.code == "CONTENT_TOO_LARGE",
      "CRLF expansion is measured against the 5 MiB serialized disk limit");
  expect(read_bytes(markdown) == original, "CRLF expansion rejection leaves the source unchanged");
}

void hard_links_and_noncanonical_editor_content_fail_closed() {
  temporary_directory temporary;
  const auto markdown = temporary.path() / "linked.md";
  const auto link = temporary.path() / "other.md";
  write_bytes(markdown, {'o', 'l', 'd'});
  std::error_code link_error;
  fs::create_hard_link(markdown, link, link_error);
  if (!link_error) {
    workspace_service service;
    const auto opened = service.open_file("window_hardlink", utf8_path(markdown));
    const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
    const auto document_id = opened.value.at("document").at("document_id").get<std::string>();
    const auto snapshot = service.open_document("window_hardlink", workspace_id, "document", document_id);
    const auto rejected = service.save_document(
        "window_hardlink",
        workspace_id,
        document_id,
        snapshot.value.at("file_version_token").get<std::string>(),
        snapshot.value.at("content_hash").get<std::string>(),
        1U,
        "preserve",
        std::vector<unsigned char>{'n', 'e', 'w'});
    expect(!rejected.ok && rejected.error.code == "UNSAFE_FILE_METADATA",
        "a multi-link file never enters replace semantics");
    expect(read_bytes(markdown) == std::vector<unsigned char>({'o', 'l', 'd'}),
        "hard-link rejection keeps every link target unchanged");
  } else {
    std::cout << "hard-link save fixture skipped because this filesystem rejected link creation\n";
  }

#ifdef _WIN32
  const auto streamed = temporary.path() / "streamed.md";
  write_bytes(streamed, {'o', 'l', 'd'});
  std::ofstream alternate_stream(utf8_path(streamed) + ":loop_metadata", std::ios::binary);
  alternate_stream << "retained metadata";
  alternate_stream.close();
  if (alternate_stream) {
    workspace_service stream_service;
    const auto stream_opened = stream_service.open_file("window_stream", utf8_path(streamed));
    const auto stream_workspace_id = stream_opened.value.at("workspace_id").get<std::string>();
    const auto stream_document_id =
        stream_opened.value.at("document").at("document_id").get<std::string>();
    const auto stream_snapshot = stream_service.open_document(
        "window_stream", stream_workspace_id, "document", stream_document_id);
    const auto stream_rejected = stream_service.save_document(
        "window_stream",
        stream_workspace_id,
        stream_document_id,
        stream_snapshot.value.at("file_version_token").get<std::string>(),
        stream_snapshot.value.at("content_hash").get<std::string>(),
        1U,
        "preserve",
        std::vector<unsigned char>{'n', 'e', 'w'});
    expect(!stream_rejected.ok && stream_rejected.error.code == "UNSAFE_FILE_METADATA",
        "Windows named streams fail closed instead of being discarded by replacement");
    expect(read_bytes(streamed) == std::vector<unsigned char>({'o', 'l', 'd'}),
        "named-stream rejection keeps the primary stream unchanged");
  } else {
    std::cout << "alternate-stream save fixture skipped because this filesystem rejected streams\n";
  }
#endif

  const auto canonical = temporary.path() / "canonical.md";
  write_bytes(canonical, {'o', 'k'});
  workspace_service canonical_service;
  const auto opened = canonical_service.open_file("window_canonical", utf8_path(canonical));
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto document_id = opened.value.at("document").at("document_id").get<std::string>();
  const auto snapshot = canonical_service.open_document(
      "window_canonical", workspace_id, "document", document_id);
  const auto carriage_return = canonical_service.save_document(
      "window_canonical",
      workspace_id,
      document_id,
      snapshot.value.at("file_version_token").get<std::string>(),
      snapshot.value.at("content_hash").get<std::string>(),
      1U,
      "preserve",
      std::vector<unsigned char>{'a', '\r', '\n'});
  expect(!carriage_return.ok && carriage_return.error.code == "INVALID_ENCODING",
      "Native rejects Renderer content that bypasses CodeMirror canonical LF form");
}

void document_capability_limit_is_enforced() {
  temporary_directory temporary;
  for (std::size_t index = 0U; index <= loop::service::k_maximum_open_documents; ++index) {
    write_bytes(temporary.path() / ("limit-" + std::to_string(index) + ".md"), {'x'});
  }
  workspace_service service;
  const auto opened = service.open_folder("window_limit", utf8_path(temporary.path()));
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto directory_id = opened.value.at("root_directory_id").get<std::string>();
  const auto page = service.list_children("window_limit", workspace_id, directory_id, "");
  expect(page.ok && page.value.at("entries").size() == loop::service::k_maximum_open_documents + 1U,
      "document limit fixture enumerates all Markdown entries");
  if (!page.ok || page.value.at("entries").size() <= loop::service::k_maximum_open_documents) return;

  for (std::size_t index = 0U; index < loop::service::k_maximum_open_documents; ++index) {
    const auto result = service.open_document(
        "window_limit",
        workspace_id,
        "entry",
        page.value.at("entries").at(index).at("entry_id").get<std::string>());
    expect(result.ok, "document capability below the per-window limit opens");
  }
  const auto overflow = service.open_document(
      "window_limit",
      workspace_id,
      "entry",
      page.value.at("entries").at(loop::service::k_maximum_open_documents).at("entry_id").get<std::string>());
  expect(!overflow.ok && overflow.error.code == "DIRECTORY_RESOURCE_LIMIT",
      "document capability above the per-window limit is rejected");
}

void rejected_bodies_do_not_consume_document_capabilities() {
  temporary_directory temporary;
  for (std::size_t index = 0U; index < loop::service::k_maximum_open_documents + 2U; ++index) {
    write_bytes(temporary.path() / ("invalid-" + std::to_string(index) + ".md"), {0xFFU});
  }
  write_bytes(temporary.path() / "valid.md", {'o', 'k'});

  workspace_service service;
  const auto opened = service.open_folder("window_invalid_limit", utf8_path(temporary.path()));
  if (!opened.ok) {
    expect(false, "invalid-body capability fixture root opens");
    return;
  }
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto root_id = opened.value.at("root_directory_id").get<std::string>();
  const auto page = service.list_children("window_invalid_limit", workspace_id, root_id, "");
  if (!page.ok) {
    expect(false, "invalid-body capability fixture enumerates");
    return;
  }
  for (const auto& entry : page.value.at("entries")) {
    const auto name = entry.at("name").get<std::string>();
    if (!name.starts_with("invalid-")) continue;
    const auto rejected = service.open_document(
        "window_invalid_limit",
        workspace_id,
        "entry",
        entry.at("entry_id").get<std::string>());
    expect(!rejected.ok && rejected.error.code == "INVALID_ENCODING",
        "an invalid body is rejected without issuing a document capability");
  }
  const auto* valid_entry = find_entry(page.value, "valid.md");
  expect(valid_entry != nullptr, "valid fixture entry is present");
  if (valid_entry == nullptr) return;
  const auto valid = service.open_document(
      "window_invalid_limit",
      workspace_id,
      "entry",
      valid_entry->at("entry_id").get<std::string>());
  expect(valid.ok, "rejected bodies do not exhaust the per-window document capability limit");
}

void failed_open_preserves_existing_capability() {
  temporary_directory temporary;
  workspace_service service;
  const auto opened = service.open_folder("window_a", utf8_path(temporary.path()));
  expect(opened.ok, "baseline folder opens");
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto directory_id = opened.value.at("root_directory_id").get<std::string>();
  const auto failed = service.open_file("window_a", utf8_path(temporary.path() / "missing.md"));
  expect(!failed.ok, "replacement open fails");
  expect(service.list_children("window_a", workspace_id, directory_id, "").ok,
      "failed replacement keeps the old capability");
}

void link_entries_are_visible_but_inaccessible_when_supported() {
  temporary_directory root;
  temporary_directory outside;
  const auto link = root.path() / "outside-link";
  if (!create_directory_link(link, outside.path())) {
    std::cout << "link boundary fixture skipped because the filesystem rejected link creation\n";
    return;
  }

  workspace_service service;
  const auto explicitly_selected = service.open_folder("window_link", utf8_path(link));
  expect(explicitly_selected.ok, "explicitly selected link can become a capability root");
  expect(explicitly_selected.ok && explicitly_selected.value.at("resolved_from_link").get<bool>(),
      "explicit root reports link resolution");
  const auto opened = service.open_folder("window_a", utf8_path(root.path()));
  const auto page = service.list_children(
      "window_a",
      opened.value.at("workspace_id").get<std::string>(),
      opened.value.at("root_directory_id").get<std::string>(),
      "");
  expect(page.ok, "folder containing link enumerates");
  const auto& entry = page.value.at("entries").front();
  expect(entry.at("kind") == "symbolic_link", "link is classified");
  expect(!entry.at("accessible").get<bool>() && !entry.at("expandable").get<bool>(),
      "link cannot expand or grant access");
  std::error_code ignored;
  fs::remove(link, ignored);
}

void renamed_root_keeps_the_explicit_object_capability() {
  temporary_directory temporary;
  workspace_service service;
  const auto opened = service.open_folder("window_a", utf8_path(temporary.path()));
  expect(opened.ok, "folder opens before rename");
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto directory_id = opened.value.at("root_directory_id").get<std::string>();
  const auto moved = temporary.path().parent_path()
      / ("loop-workspace-moved-" + loop::support::secure_random_hex(8U));

  std::error_code rename_error;
  fs::rename(temporary.path(), moved, rename_error);
  if (rename_error) {
    std::cout << "rename invalidation fixture skipped: " << rename_error.message() << '\n';
    return;
  }
  const auto listed = service.list_children("window_a", workspace_id, directory_id, "");
  expect(listed.ok, "renamed root keeps the explicitly selected root object capability");
  fs::rename(moved, temporary.path(), rename_error);
  expect(!rename_error, "renamed test root is restored");
}

void root_capability_is_move_only_and_rejects_regular_files() {
  temporary_directory temporary;
  const auto markdown = temporary.path() / "not-a-folder.md";
  write_bytes(markdown, {'x'});

  loop::service::filesystem_capability_port port;
  auto regular_file = port.open_root(markdown);
  expect(!regular_file.ok && regular_file.error_code == "NOT_DIRECTORY",
      "a regular file cannot become a folder root capability");

  auto opened = port.open_root(temporary.path());
  expect(opened.ok && opened.root.valid(), "a folder root capability owns a valid platform handle");
  if (!opened.ok) return;
  auto moved = std::move(opened.root);
  expect(!opened.root.valid() && moved.valid(), "moving a root capability transfers its only handle owner");
  const auto listed = port.list_directory(moved, {}, opened.identity, 10U, 4096U);
  expect(listed.ok, "the moved root capability remains usable");
  const auto entry_limited = port.list_directory(moved, {}, opened.identity, 0U, 4096U);
  expect(!entry_limited.ok && entry_limited.error_code == "DIRECTORY_RESOURCE_LIMIT",
      "the port enforces the entry limit while enumerating");
  const auto metadata_limited = port.list_directory(moved, {}, opened.identity, 10U, 1U);
  expect(!metadata_limited.ok && metadata_limited.error_code == "DIRECTORY_RESOURCE_LIMIT",
      "the port enforces the metadata limit while enumerating");
}

void handle_relative_safe_replace_updates_identity_and_rejects_conflicts() {
  temporary_directory temporary;
  const auto markdown = temporary.path() / "save.md";
  const std::vector<unsigned char> original{'o', 'l', 'd', '\n'};
  const std::vector<unsigned char> replacement{'n', 'e', 'w', '\n'};
  write_bytes(markdown, original);

  loop::service::filesystem_capability_port port;
  auto root = port.open_root(temporary.path());
  expect(root.ok, "safe-replace fixture root opens");
  if (!root.ok) return;
  const loop::service::filesystem_component_chain components{"save.md"};
  auto authorized = port.authorize_regular_file(root.root, components);
  expect(authorized.ok, "safe-replace fixture authorizes the selected regular file");
  if (!authorized.ok) return;
  const auto original_identity = authorized.identity;
  const auto replaced = port.replace_regular_file(
      root.root,
      components,
      original_identity,
      loop::support::sha256_hex(original),
      replacement);
  if (!replaced.ok) {
    std::cerr << "safe replace diagnostic: " << replaced.error_code << '\n';
  }
  expect(replaced.ok && replaced.identity != original_identity,
      "safe replace atomically installs a new file identity");
  if (!replaced.ok) return;
  std::ifstream input(markdown, std::ios::binary);
  const std::string disk{
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>()};
  expect(disk == std::string(replacement.begin(), replacement.end()),
      "safe replace writes the exact requested bytes");

  const auto stale = port.replace_regular_file(
      root.root,
      components,
      replaced.identity,
      loop::support::sha256_hex(original),
      original);
  expect(!stale.ok && stale.error_code == "DOCUMENT_CONFLICT",
      "safe replace rejects a stale content hash without changing the file");
}

void safe_replace_revalidates_the_target_immediately_before_commit() {
  temporary_directory temporary;
  const auto markdown = temporary.path() / "race.md";
  const auto moved = temporary.path() / "race-original.md";
  const std::vector<unsigned char> original{'o', 'l', 'd'};
  const std::vector<unsigned char> external{'e', 'x', 't'};
  write_bytes(markdown, original);
  bool injected = false;
  loop::service::filesystem_capability_port port([&]() {
    std::error_code error;
    fs::rename(markdown, moved, error);
    if (!error) {
      write_bytes(markdown, external);
      injected = true;
    }
  });
  auto root = port.open_root(temporary.path());
  expect(root.ok, "pre-commit race fixture root opens");
  if (!root.ok) return;
  const loop::service::filesystem_component_chain components{"race.md"};
  auto authorized = port.authorize_regular_file(root.root, components);
  expect(authorized.ok, "pre-commit race fixture authorizes the original target");
  if (!authorized.ok) return;
  const auto replaced = port.replace_regular_file(
      root.root,
      components,
      authorized.identity,
      loop::support::sha256_hex(original),
      std::vector<unsigned char>{'n', 'e', 'w'});
  expect(injected, "pre-commit race injection replaces the pathname after metadata copy");
  expect(!replaced.ok && replaced.error_code == "WORKSPACE_INVALID",
      "final root-relative validation rejects a target identity replaced before commit");
  expect(read_bytes(markdown) == external && read_bytes(moved) == original,
      "failed pre-commit validation preserves both external and original file contents");
}

void moved_descendant_invalidates_old_directory_and_document_capabilities() {
  temporary_directory root;
  temporary_directory outside;
  const auto parent = root.path() / "parent";
  fs::create_directory(parent);
  write_bytes(parent / "note.md", {'o', 'l', 'd'});

  workspace_service service;
  const auto opened = service.open_folder("window_move", utf8_path(root.path()));
  expect(opened.ok, "descendant-move fixture root opens");
  if (!opened.ok) return;
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto root_id = opened.value.at("root_directory_id").get<std::string>();
  const auto root_page = service.list_children("window_move", workspace_id, root_id, "");
  const auto* parent_entry = root_page.ok ? find_entry(root_page.value, "parent") : nullptr;
  expect(parent_entry != nullptr, "descendant directory receives an opaque capability");
  if (parent_entry == nullptr) return;
  const auto parent_id = parent_entry->at("entry_id").get<std::string>();
  const auto child_page = service.list_children("window_move", workspace_id, parent_id, "");
  const auto* note_entry = child_page.ok ? find_entry(child_page.value, "note.md") : nullptr;
  expect(note_entry != nullptr, "descendant Markdown receives an opaque capability");
  if (note_entry == nullptr) return;
  const auto note_id = note_entry->at("entry_id").get<std::string>();

  const auto moved_parent = outside.path() / "moved-parent";
  std::error_code rename_error;
  fs::rename(parent, moved_parent, rename_error);
  if (rename_error) {
    std::cout << "descendant move fixture skipped because rename failed: "
              << rename_error.message() << '\n';
    return;
  }
  const auto old_directory = service.list_children("window_move", workspace_id, parent_id, "");
  expect(!old_directory.ok, "a directory moved outside the root invalidates its old component chain");
  const auto old_document = service.open_document("window_move", workspace_id, "entry", note_id);
  expect(!old_document.ok && !old_document.body_present,
      "a document moved outside the root cannot return its body through an old capability");
}

void replaced_file_identity_invalidates_the_old_entry_capability() {
  temporary_directory temporary;
  const auto markdown = temporary.path() / "replace.md";
  const auto displaced = temporary.path() / "displaced.md";
  write_bytes(markdown, {'o', 'l', 'd'});

  workspace_service service;
  const auto opened = service.open_folder("window_replace", utf8_path(temporary.path()));
  if (!opened.ok) {
    expect(false, "replacement fixture root opens");
    return;
  }
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto root_id = opened.value.at("root_directory_id").get<std::string>();
  const auto page = service.list_children("window_replace", workspace_id, root_id, "");
  const auto* entry = page.ok ? find_entry(page.value, "replace.md") : nullptr;
  expect(entry != nullptr, "replacement fixture receives an entry capability");
  if (entry == nullptr) return;
  const auto entry_id = entry->at("entry_id").get<std::string>();

  std::error_code rename_error;
  fs::rename(markdown, displaced, rename_error);
  if (rename_error) {
    std::cout << "identity replacement fixture skipped because rename failed: "
              << rename_error.message() << '\n';
    return;
  }
  write_bytes(markdown, {'n', 'e', 'w'});
  const auto stale = service.open_document("window_replace", workspace_id, "entry", entry_id);
  expect(!stale.ok && stale.error.code == "WORKSPACE_INVALID" && !stale.body_present,
      "a same-name replacement cannot reuse the old file identity capability");
}

void intermediate_junction_swap_fails_closed_when_supported() {
  temporary_directory root;
  temporary_directory outside;
  const auto parent = root.path() / "parent";
  fs::create_directory(parent);
  write_bytes(parent / "note.md", {'o', 'l', 'd'});

  workspace_service service;
  const auto opened = service.open_folder("window_swap", utf8_path(root.path()));
  if (!opened.ok) {
    expect(false, "junction-swap fixture root opens");
    return;
  }
  const auto workspace_id = opened.value.at("workspace_id").get<std::string>();
  const auto root_id = opened.value.at("root_directory_id").get<std::string>();
  const auto root_page = service.list_children("window_swap", workspace_id, root_id, "");
  const auto* parent_entry = root_page.ok ? find_entry(root_page.value, "parent") : nullptr;
  if (parent_entry == nullptr) {
    expect(false, "junction-swap directory receives a capability");
    return;
  }
  const auto parent_id = parent_entry->at("entry_id").get<std::string>();
  const auto child_page = service.list_children("window_swap", workspace_id, parent_id, "");
  const auto* note_entry = child_page.ok ? find_entry(child_page.value, "note.md") : nullptr;
  if (note_entry == nullptr) {
    expect(false, "junction-swap Markdown receives a capability");
    return;
  }
  const auto note_id = note_entry->at("entry_id").get<std::string>();

  const auto moved_parent = outside.path() / "moved-parent";
  std::error_code rename_error;
  fs::rename(parent, moved_parent, rename_error);
  if (rename_error || !create_directory_link(parent, moved_parent)) {
    std::cout << "junction-swap fixture skipped because this filesystem rejected the swap\n";
    return;
  }
  const auto stale = service.open_document("window_swap", workspace_id, "entry", note_id);
  expect(!stale.ok && !stale.body_present,
      "an intermediate junction exchange cannot redirect an old document capability");
}

void unc_root_fails_closed_when_available() {
  temporary_directory temporary;
  const auto absolute = fs::absolute(temporary.path());
  auto root_name = utf8_path(absolute.root_name());
  if (root_name.size() != 2U || root_name[1] != ':') {
    std::cout << "UNC fixture skipped on non-drive filesystem\n";
    return;
  }
  root_name[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(root_name[0])));
  const auto unc_root = path_from_utf8_for_test(
      "\\\\localhost\\" + root_name.substr(0U, 1U) + "$\\" + utf8_path(absolute.relative_path()));
  std::error_code access_error;
  if (!fs::is_directory(unc_root, access_error) || access_error) {
    std::cout << "UNC fixture skipped because the local administrative share is unavailable\n";
    return;
  }

  workspace_service service;
  const auto opened = service.open_folder("window_unc", utf8_path(unc_root));
  if (opened.ok || opened.error.code != "INTERNAL_ERROR") {
    std::cerr << "UNC diagnostic: ok=" << opened.ok
              << " code=" << opened.error.code
              << " locator=" << utf8_path(unc_root) << '\n';
  }
  expect(!opened.ok && opened.error.code == "INTERNAL_ERROR",
      "UNC folder fails closed when a local handle-relative identity cannot be guaranteed");
}

void drive_letter_case_does_not_change_identity_when_available() {
  temporary_directory temporary;
  const auto absolute = fs::absolute(temporary.path());
  auto root_name = utf8_path(absolute.root_name());
  if (root_name.size() != 2U || root_name[1] != ':') return;
  const auto original = static_cast<unsigned char>(root_name[0]);
  root_name[0] = std::islower(original)
      ? static_cast<char>(std::toupper(original))
      : static_cast<char>(std::tolower(original));
  const auto alternate = path_from_utf8_for_test(root_name + "\\" + utf8_path(absolute.relative_path()));

  workspace_service service;
  const auto opened = service.open_folder("window_case", utf8_path(alternate));
  expect(opened.ok, "drive-letter case variant opens the same directory identity");
  if (!opened.ok) return;
  expect(service.list_children(
      "window_case",
      opened.value.at("workspace_id").get<std::string>(),
      opened.value.at("root_directory_id").get<std::string>(),
      "").ok,
      "drive-letter case variant remains within the capability root");
}

void ten_thousand_entry_directory_meets_browse_target() {
  temporary_directory temporary;
  for (int index = 0; index < 10'000; ++index) {
    write_bytes(temporary.path() / ("entry-" + std::to_string(index) + ".md"), {});
  }

  workspace_service service;
  const auto opened = service.open_folder("window_a", utf8_path(temporary.path()));
  const auto started = std::chrono::steady_clock::now();
  const auto page = service.list_children(
      "window_a",
      opened.value.at("workspace_id").get<std::string>(),
      opened.value.at("root_directory_id").get<std::string>(),
      "");
  const auto elapsed = std::chrono::steady_clock::now() - started;
  const auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  std::cout << "10000-entry enumeration: " << elapsed_milliseconds << " ms\n";
  expect(page.ok && page.value.at("total_entries") == 10'000, "10000 entry directory is enumerated");
  expect(elapsed <= std::chrono::seconds(2), "10000 entry directory is browsable within 2 seconds");
}

}  // namespace

int main() {
  markdown_inspection_is_strict();
  sha256_matches_standard_vector();
  capability_child_names_are_single_components();
  opening_file_validates_content_and_hides_locator();
  folder_capability_is_window_bound_and_paginated();
  document_reads_are_handle_bound_and_capability_scoped();
  document_reads_reject_path_mutation_after_handle_read();
  document_save_preserves_format_rotates_tokens_and_detects_conflicts();
  mixed_line_endings_require_an_explicit_normalization_choice();
  crlf_expansion_cannot_exceed_the_disk_byte_limit();
  hard_links_and_noncanonical_editor_content_fail_closed();
  document_capability_limit_is_enforced();
  rejected_bodies_do_not_consume_document_capabilities();
  failed_open_preserves_existing_capability();
  link_entries_are_visible_but_inaccessible_when_supported();
  renamed_root_keeps_the_explicit_object_capability();
  root_capability_is_move_only_and_rejects_regular_files();
  handle_relative_safe_replace_updates_identity_and_rejects_conflicts();
  safe_replace_revalidates_the_target_immediately_before_commit();
  moved_descendant_invalidates_old_directory_and_document_capabilities();
  replaced_file_identity_invalidates_the_old_entry_capability();
  intermediate_junction_swap_fails_closed_when_supported();
  unc_root_fails_closed_when_available();
  drive_letter_case_does_not_change_identity_when_available();
  ten_thousand_entry_directory_meets_browse_target();
  if (failures == 0) {
    std::cout << "all workspace tests passed\n";
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
