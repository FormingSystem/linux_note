#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "service/filesystem_capability_port.h"

namespace loop::service {

inline constexpr std::size_t k_maximum_markdown_bytes = 5U * 1024U * 1024U;

struct markdown_inspection {
  bool bom = false;
  std::string line_ending = "none";
};

struct file_read_result {
  bool ok = false;
  std::string error_code;
  std::filesystem::path canonical_path;
  std::string identity;
  std::vector<unsigned char> raw_bytes;
  std::vector<unsigned char> content_bytes;
  std::string content_hash;
  markdown_inspection inspection;
  std::uint64_t byte_size = 0U;
  std::uint64_t modified_time_ms = 0U;
  std::uint64_t link_count = 0U;
  bool read_only = false;
};

[[nodiscard]] bool inspect_markdown_bytes(
    std::span<const unsigned char> bytes,
    markdown_inspection& inspection);

class file_service {
 public:
  using after_read_observer = std::function<void()>;

  explicit file_service(after_read_observer after_read = {});

  [[nodiscard]] file_read_result read_markdown(
      const std::filesystem::path& canonical_path,
      std::string_view expected_identity) const;

  [[nodiscard]] file_read_result read_markdown(filesystem_capability_file file) const;

 private:
  after_read_observer after_read_;
};

}  // namespace loop::service
