#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace loop::service {

inline constexpr std::size_t k_maximum_markdown_bytes = 5U * 1024U * 1024U;
inline constexpr std::size_t k_maximum_directory_entries = 50'000U;
inline constexpr std::size_t k_maximum_directory_metadata_bytes = 32U * 1024U * 1024U;
inline constexpr std::size_t k_maximum_entries_per_page = 256U;

struct markdown_inspection {
  bool bom = false;
  std::string line_ending = "none";
};

struct service_error {
  std::string code;
  std::string user_message;
  bool retryable = false;
  std::vector<std::string> recovery_actions;
};

struct service_result {
  bool ok = false;
  nlohmann::json value = nlohmann::json::object();
  service_error error;

  [[nodiscard]] static service_result success(nlohmann::json value);
  [[nodiscard]] static service_result failure(service_error error);
};

[[nodiscard]] bool inspect_markdown_bytes(
    std::span<const unsigned char> bytes,
    markdown_inspection& inspection);

class workspace_service {
 public:
  workspace_service();
  ~workspace_service();
  workspace_service(workspace_service&&) noexcept;
  workspace_service& operator=(workspace_service&&) noexcept;
  workspace_service(const workspace_service&) = delete;
  workspace_service& operator=(const workspace_service&) = delete;

  [[nodiscard]] service_result open_file(
      std::string_view window_session_id,
      std::string_view locator);
  [[nodiscard]] service_result open_folder(
      std::string_view window_session_id,
      std::string_view locator);
  [[nodiscard]] service_result close(std::string_view window_session_id);
  [[nodiscard]] service_result list_children(
      std::string_view window_session_id,
      std::string_view workspace_id,
      std::string_view directory_id,
      std::string_view cursor);

 private:
  class impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace loop::service
