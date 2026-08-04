#include "protocol/framing.h"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace loop::protocol {
namespace {

constexpr std::array<unsigned char, 4U> k_magic{'L', 'O', 'O', 'P'};
constexpr unsigned char k_frame_version = 1U;
constexpr unsigned char k_body_present_flag = 0x01U;

std::uint32_t read_u32(const std::span<const unsigned char> bytes) {
  return (static_cast<std::uint32_t>(bytes[0]) << 24U)
      | (static_cast<std::uint32_t>(bytes[1]) << 16U)
      | (static_cast<std::uint32_t>(bytes[2]) << 8U)
      | static_cast<std::uint32_t>(bytes[3]);
}

void append_u32(std::vector<unsigned char>& output, const std::size_t value) {
  if (value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    throw std::runtime_error("frame length exceeds uint32");
  }
  const auto converted = static_cast<std::uint32_t>(value);
  output.push_back(static_cast<unsigned char>((converted >> 24U) & 0xFFU));
  output.push_back(static_cast<unsigned char>((converted >> 16U) & 0xFFU));
  output.push_back(static_cast<unsigned char>((converted >> 8U) & 0xFFU));
  output.push_back(static_cast<unsigned char>(converted & 0xFFU));
}

void validate_lengths(
    const std::size_t control_length,
    const std::size_t body_length,
    const bool body_present) {
  if (control_length == 0U || control_length > k_max_control_frame_bytes) {
    throw std::runtime_error("invalid control frame length");
  }
  if (body_length > k_max_body_frame_bytes || (!body_present && body_length != 0U)) {
    throw std::runtime_error("invalid body frame length");
  }
}

}  // namespace

std::vector<unsigned char> encode_frame(const transport_frame& frame) {
  validate_lengths(frame.control.size(), frame.body.size(), frame.body_present);
  std::vector<unsigned char> output;
  output.reserve(k_frame_header_bytes + frame.control.size() + frame.body.size());
  output.insert(output.end(), k_magic.begin(), k_magic.end());
  output.push_back(k_frame_version);
  output.push_back(frame.body_present ? k_body_present_flag : 0U);
  output.push_back(0U);
  output.push_back(0U);
  append_u32(output, frame.control.size());
  append_u32(output, frame.body.size());
  output.insert(output.end(), frame.control.begin(), frame.control.end());
  output.insert(output.end(), frame.body.begin(), frame.body.end());
  return output;
}

std::vector<transport_frame> frame_decoder::push(const std::span<const unsigned char> bytes) {
  if (!bytes.empty()) buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());

  std::vector<transport_frame> frames;
  while (buffer_.size() >= k_frame_header_bytes) {
    for (std::size_t index = 0U; index < k_magic.size(); ++index) {
      if (buffer_[index] != k_magic[index]) throw std::runtime_error("invalid frame magic");
    }
    if (buffer_[4] != k_frame_version || (buffer_[5] & ~k_body_present_flag) != 0U
        || buffer_[6] != 0U || buffer_[7] != 0U) {
      throw std::runtime_error("invalid frame header");
    }
    const auto body_present = (buffer_[5] & k_body_present_flag) != 0U;
    const auto control_length = static_cast<std::size_t>(read_u32(
        std::span<const unsigned char>(buffer_.data() + 8U, 4U)));
    const auto body_length = static_cast<std::size_t>(read_u32(
        std::span<const unsigned char>(buffer_.data() + 12U, 4U)));
    validate_lengths(control_length, body_length, body_present);

    const auto total_length = k_frame_header_bytes + control_length + body_length;
    if (buffer_.size() < total_length) break;

    transport_frame frame;
    frame.body_present = body_present;
    const auto control_begin = buffer_.begin() + static_cast<std::ptrdiff_t>(k_frame_header_bytes);
    const auto control_end = control_begin + static_cast<std::ptrdiff_t>(control_length);
    frame.control.assign(control_begin, control_end);
    if (body_present) {
      frame.body.assign(
          control_end,
          control_end + static_cast<std::ptrdiff_t>(body_length));
    }
    frames.push_back(std::move(frame));
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(total_length));
  }

  const auto maximum_buffer = k_frame_header_bytes + k_max_control_frame_bytes + k_max_body_frame_bytes;
  if (buffer_.size() > maximum_buffer) throw std::runtime_error("frame buffer exceeds limit");
  return frames;
}

bool frame_decoder::finish() const {
  return buffer_.empty();
}

}  // namespace loop::protocol
