#include "protocol/framing.h"

#include <istream>
#include <ostream>
#include <utility>

namespace loop::protocol {

read_result read_frame(std::istream& input, const std::size_t maximum_payload_bytes) {
  std::string payload;
  payload.reserve(4096U);
  while (true) {
    const auto value = input.get();
    if (value == std::char_traits<char>::eof()) {
      if (payload.empty() && input.eof()) return {read_status::end_of_stream, {}};
      return {read_status::error, {}};
    }
    if (value == '\n') break;
    if (payload.size() >= maximum_payload_bytes) return {read_status::error, {}};
    payload.push_back(static_cast<char>(value));
  }
  if (!payload.empty() && payload.back() == '\r') payload.pop_back();
  if (payload.empty()) return {read_status::error, {}};
  return {read_status::frame, std::move(payload)};
}

bool write_frame(
    std::ostream& output,
    const std::string& payload,
    const std::size_t maximum_payload_bytes) {
  if (payload.empty() || payload.size() > maximum_payload_bytes
      || payload.find_first_of("\r\n") != std::string::npos) {
    return false;
  }
  output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  output.put('\n');
  output.flush();
  return output.good();
}

}  // namespace loop::protocol
