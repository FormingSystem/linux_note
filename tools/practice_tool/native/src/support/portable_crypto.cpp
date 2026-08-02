#include "support/portable_crypto.h"

#include <array>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/sha256.h>

namespace loop::support {
namespace {

std::string to_hex(const std::span<const unsigned char> bytes) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto byte : bytes) output << std::setw(2) << static_cast<unsigned int>(byte);
  return output.str();
}

class portable_random_generator {
 public:
  portable_random_generator() {
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&generator_);
    constexpr std::array<unsigned char, 28> personalization{
        'l', 'o', 'o', 'p', '-', 'w', 'o', 'r', 'k', 's', 'p', 'a', 'c', 'e', '-',
        'c', 'a', 'p', 'a', 'b', 'i', 'l', 'i', 't', 'y', '-', 'v', '1'};
    if (mbedtls_ctr_drbg_seed(
            &generator_,
            mbedtls_entropy_func,
            &entropy_,
            personalization.data(),
            personalization.size()) != 0) {
      mbedtls_ctr_drbg_free(&generator_);
      mbedtls_entropy_free(&entropy_);
      throw std::runtime_error("secure random generator initialization failed");
    }
  }

  ~portable_random_generator() {
    mbedtls_ctr_drbg_free(&generator_);
    mbedtls_entropy_free(&entropy_);
  }

  portable_random_generator(const portable_random_generator&) = delete;
  portable_random_generator& operator=(const portable_random_generator&) = delete;

  void fill(const std::span<unsigned char> bytes) {
    const std::scoped_lock lock(mutex_);
    if (mbedtls_ctr_drbg_random(&generator_, bytes.data(), bytes.size()) != 0) {
      throw std::runtime_error("secure random generator failed");
    }
  }

 private:
  mbedtls_entropy_context entropy_;
  mbedtls_ctr_drbg_context generator_;
  std::mutex mutex_;
};

}  // namespace

std::string secure_random_hex(const std::size_t byte_count) {
  if (byte_count == 0U || byte_count > 64U) throw std::invalid_argument("invalid random byte count");

  std::vector<unsigned char> bytes(byte_count);
  static portable_random_generator generator;
  generator.fill(bytes);
  return to_hex(bytes);
}

std::string sha256_hex(const std::span<const unsigned char> bytes) {
  std::array<unsigned char, 32> digest{};
  if (mbedtls_sha256(bytes.data(), bytes.size(), digest.data(), 0) != 0) {
    throw std::runtime_error("SHA-256 hashing failed");
  }
  return to_hex(digest);
}

}  // namespace loop::support
