#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace loop::support {

[[nodiscard]] std::string secure_random_hex(std::size_t byte_count);
[[nodiscard]] std::string sha256_hex(std::span<const unsigned char> bytes);

}  // namespace loop::support
