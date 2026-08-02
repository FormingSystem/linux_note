#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "service/workspace_service.h"
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

  const std::vector<unsigned char> mixed{'a', '\r', '\n', 'b', '\n'};
  expect(loop::service::inspect_markdown_bytes(mixed, inspection), "mixed line ending markdown is accepted");
  expect(inspection.line_ending == "mixed", "mixed line endings are reported");

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
  expect(service.open_file("window_a", utf8_path(exact_limit)).ok, "a file at the 5 MiB limit opens");

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

void renamed_root_invalidates_capability() {
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
  expect(!listed.ok && listed.error.code == "WORKSPACE_INVALID", "renamed root invalidates capability");
  fs::rename(moved, temporary.path(), rename_error);
  expect(!rename_error, "renamed test root is restored");
}

void unc_root_uses_the_same_portable_path_flow_when_available() {
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
  expect(opened.ok, "UNC folder opens through portable filesystem APIs");
  if (!opened.ok) return;
  const auto listed = service.list_children(
      "window_unc",
      opened.value.at("workspace_id").get<std::string>(),
      opened.value.at("root_directory_id").get<std::string>(),
      "");
  expect(listed.ok, "UNC root enumerates");
  expect(opened.value.dump().find("localhost") == std::string::npos, "UNC locator is not returned");
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
  opening_file_validates_content_and_hides_locator();
  folder_capability_is_window_bound_and_paginated();
  failed_open_preserves_existing_capability();
  link_entries_are_visible_but_inaccessible_when_supported();
  renamed_root_invalidates_capability();
  unc_root_uses_the_same_portable_path_flow_when_available();
  drive_letter_case_does_not_change_identity_when_available();
  ten_thousand_entry_directory_meets_browse_target();
  if (failures == 0) {
    std::cout << "all workspace tests passed\n";
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
