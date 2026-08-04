#pragma once

#include "protocol/framing.h"
#include "service/workspace_service.h"

namespace loop::protocol {

inline constexpr int k_protocol_version = 4;

class request_handler {
 public:
  [[nodiscard]] transport_frame handle_request(const transport_frame& request);

 private:
  loop::service::workspace_service workspace_service_;
};

}  // namespace loop::protocol
