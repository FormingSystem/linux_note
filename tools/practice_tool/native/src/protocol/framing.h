#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

namespace loop::protocol {

inline constexpr std::size_t k_max_control_frame_bytes = 1024U * 1024U;

enum class read_status {
  frame,
  end_of_stream,
  error,
};

struct read_result {
  read_status status;
  std::string payload;
};

[[nodiscard]] read_result read_frame(
    std::istream& input,
    std::size_t maximum_payload_bytes = k_max_control_frame_bytes);

[[nodiscard]] bool write_frame(
    std::ostream& output,
    const std::string& payload,
    std::size_t maximum_payload_bytes = k_max_control_frame_bytes);

}  // namespace loop::protocol
