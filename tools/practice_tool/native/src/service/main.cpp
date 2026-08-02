#include <cstdlib>
#include <iostream>
#include <string_view>

#include "protocol/framing.h"
#include "protocol/messages.h"

int main(const int argc, const char* const argv[]) {
  if (argc != 2 || std::string_view(argv[1]) != "--stdio") {
    std::cerr << "usage: loop_native_service --stdio\n";
    return EXIT_FAILURE;
  }

  loop::protocol::request_handler request_handler;
  while (true) {
    const auto frame = loop::protocol::read_frame(std::cin);
    if (frame.status == loop::protocol::read_status::end_of_stream) return EXIT_SUCCESS;
    if (frame.status == loop::protocol::read_status::error) return EXIT_FAILURE;

    const auto response = request_handler.handle_request(frame.payload);
    if (!loop::protocol::write_frame(std::cout, response)) return EXIT_FAILURE;
  }
}
