#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace loop::protocol {

inline constexpr std::size_t k_frame_header_bytes = 16U;
inline constexpr std::size_t k_max_control_frame_bytes = 1024U * 1024U;
inline constexpr std::size_t k_max_body_frame_bytes = 5U * 1024U * 1024U;

struct transport_frame {
  std::string control;
  std::vector<unsigned char> body;
  bool body_present = false;
};

[[nodiscard]] std::vector<unsigned char> encode_frame(const transport_frame& frame);

class frame_decoder {
 public:
  [[nodiscard]] std::vector<transport_frame> push(std::span<const unsigned char> bytes);
  [[nodiscard]] bool finish() const;

 private:
  std::vector<unsigned char> buffer_;
};

}  // namespace loop::protocol
