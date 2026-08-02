#include "protocol/framing.h"

#include <array>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <utility>

namespace loop::protocol {
namespace {

constexpr std::size_t kHeaderBytes = 4U;

std::uint32_t decode_length(const std::array<unsigned char, kHeaderBytes>& header) {
  return (static_cast<std::uint32_t>(header[0]) << 24U)
      | (static_cast<std::uint32_t>(header[1]) << 16U)
      | (static_cast<std::uint32_t>(header[2]) << 8U)
      | static_cast<std::uint32_t>(header[3]);
}

}  // namespace

ReadResult read_frame(std::istream& input, const std::size_t maximum_payload_bytes) {
  std::array<unsigned char, kHeaderBytes> header{};
  input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
  if (input.gcount() == 0 && input.eof()) return {ReadStatus::end_of_stream, {}};
  if (input.gcount() != static_cast<std::streamsize>(header.size())) return {ReadStatus::error, {}};

  const auto payload_length = static_cast<std::size_t>(decode_length(header));
  if (payload_length == 0U || payload_length > maximum_payload_bytes) return {ReadStatus::error, {}};
  if (payload_length > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    return {ReadStatus::error, {}};
  }

  std::string payload(payload_length, '\0');
  input.read(payload.data(), static_cast<std::streamsize>(payload.size()));
  if (input.gcount() != static_cast<std::streamsize>(payload.size())) return {ReadStatus::error, {}};
  return {ReadStatus::frame, std::move(payload)};
}

bool write_frame(
    std::ostream& output,
    const std::string& payload,
    const std::size_t maximum_payload_bytes) {
  if (payload.empty() || payload.size() > maximum_payload_bytes
      || payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return false;
  }

  const auto length = static_cast<std::uint32_t>(payload.size());
  const std::array<unsigned char, kHeaderBytes> header{
      static_cast<unsigned char>((length >> 24U) & 0xFFU),
      static_cast<unsigned char>((length >> 16U) & 0xFFU),
      static_cast<unsigned char>((length >> 8U) & 0xFFU),
      static_cast<unsigned char>(length & 0xFFU),
  };
  output.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
  output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  output.flush();
  return output.good();
}

}  // namespace loop::protocol
