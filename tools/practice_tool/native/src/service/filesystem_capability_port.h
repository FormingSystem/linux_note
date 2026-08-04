#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace loop::service {

enum class filesystem_capability_platform {
  linux,
  windows,
};

enum class filesystem_capability_entry_kind {
  directory,
  regular_file,
  symbolic_link,
  other,
};

using filesystem_component_chain = std::vector<std::string>;

[[nodiscard]] bool is_safe_capability_child_name(
    std::string_view child_name,
    filesystem_capability_platform platform);

[[nodiscard]] bool is_safe_native_capability_child_name(std::string_view child_name);

class filesystem_workspace_root {
 public:
  filesystem_workspace_root();
  ~filesystem_workspace_root();
  filesystem_workspace_root(filesystem_workspace_root&&) noexcept;
  filesystem_workspace_root& operator=(filesystem_workspace_root&&) noexcept;
  filesystem_workspace_root(const filesystem_workspace_root&) = delete;
  filesystem_workspace_root& operator=(const filesystem_workspace_root&) = delete;

  [[nodiscard]] bool valid() const;

 private:
  class impl;
  std::unique_ptr<impl> impl_;
  friend class filesystem_capability_port;
};

class filesystem_capability_file {
 public:
  filesystem_capability_file();
  ~filesystem_capability_file();
  filesystem_capability_file(filesystem_capability_file&&) noexcept;
  filesystem_capability_file& operator=(filesystem_capability_file&&) noexcept;
  filesystem_capability_file(const filesystem_capability_file&) = delete;
  filesystem_capability_file& operator=(const filesystem_capability_file&) = delete;

  [[nodiscard]] bool valid() const;
  [[nodiscard]] int descriptor() const;

 private:
  explicit filesystem_capability_file(int descriptor);
  int descriptor_ = -1;
  friend class filesystem_capability_port;
};

struct filesystem_root_open_result {
  bool ok = false;
  std::string error_code;
  std::string identity;
  filesystem_workspace_root root;
};

struct filesystem_capability_entry {
  std::string name;
  std::string display_name;
  std::string identity;
  filesystem_capability_entry_kind kind = filesystem_capability_entry_kind::other;
  std::optional<std::uint64_t> byte_size;
  bool accessible = false;
};

struct filesystem_directory_result {
  bool ok = false;
  std::string error_code;
  std::vector<filesystem_capability_entry> entries;
};

struct filesystem_file_open_result {
  bool ok = false;
  std::string error_code;
  std::string identity;
  filesystem_capability_file file;
};

struct filesystem_replace_result {
  bool ok = false;
  std::string error_code;
  std::string identity;
};

class filesystem_capability_port {
 public:
  explicit filesystem_capability_port(std::function<void()> before_replace_validation = {});
  [[nodiscard]] filesystem_root_open_result open_root(
      const std::filesystem::path& selected_path) const;

  [[nodiscard]] filesystem_directory_result list_directory(
      const filesystem_workspace_root& root,
      const filesystem_component_chain& components,
      std::string_view expected_identity,
      std::size_t maximum_entries,
      std::size_t maximum_metadata_bytes) const;

  [[nodiscard]] filesystem_file_open_result open_regular_file(
      const filesystem_workspace_root& root,
      const filesystem_component_chain& components,
      std::string_view expected_identity) const;

  [[nodiscard]] filesystem_file_open_result authorize_regular_file(
      const filesystem_workspace_root& root,
      const filesystem_component_chain& components) const;

  [[nodiscard]] bool verify_regular_file(
      const filesystem_workspace_root& root,
      const filesystem_component_chain& components,
      std::string_view expected_identity) const;

  [[nodiscard]] filesystem_replace_result replace_regular_file(
      const filesystem_workspace_root& root,
      const filesystem_component_chain& components,
      std::string_view expected_identity,
      std::string_view expected_content_hash,
      std::span<const unsigned char> replacement_bytes) const;

 private:
  std::function<void()> before_replace_validation_;
};

}  // namespace loop::service
