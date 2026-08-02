#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

namespace loop::protocol {

inline constexpr std::size_t kMaxControlFrameBytes = 1024U * 1024U;

enum class ReadStatus {
  frame,
  end_of_stream,
  error,
};

struct ReadResult {
  ReadStatus status;
  std::string payload;
};

[[nodiscard]] ReadResult read_frame(
    std::istream& input,
    std::size_t maximum_payload_bytes = kMaxControlFrameBytes);

[[nodiscard]] bool write_frame(
    std::ostream& output,
    const std::string& payload,
    std::size_t maximum_payload_bytes = kMaxControlFrameBytes);

}  // namespace loop::protocol
