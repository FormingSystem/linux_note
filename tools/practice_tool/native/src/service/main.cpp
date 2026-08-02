#include <cstdlib>
#include <iostream>
#include <string_view>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "protocol/framing.h"
#include "protocol/messages.h"

namespace {

void configure_binary_stdio() {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
  (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
}

}  // namespace

int main(const int argc, const char* const argv[]) {
  if (argc != 2 || std::string_view(argv[1]) != "--stdio") {
    std::cerr << "usage: loop_native_service --stdio\n";
    return EXIT_FAILURE;
  }

  configure_binary_stdio();
  while (true) {
    const auto frame = loop::protocol::read_frame(std::cin);
    if (frame.status == loop::protocol::ReadStatus::end_of_stream) return EXIT_SUCCESS;
    if (frame.status == loop::protocol::ReadStatus::error) return EXIT_FAILURE;

    const auto response = loop::protocol::handle_request(frame.payload);
    if (!loop::protocol::write_frame(std::cout, response)) return EXIT_FAILURE;
  }
}
