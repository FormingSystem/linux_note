#pragma once

#include <string>
#include <string_view>

namespace loop::protocol {

inline constexpr int kProtocolVersion = 1;

[[nodiscard]] std::string handle_request(std::string_view payload);

}  // namespace loop::protocol
