#pragma once

#include <string>
#include <string_view>

#include "service/workspace_service.h"

namespace loop::protocol {

inline constexpr int k_protocol_version = 2;

class request_handler {
 public:
  [[nodiscard]] std::string handle_request(std::string_view payload);

 private:
  loop::service::workspace_service workspace_service_;
};

}  // namespace loop::protocol
