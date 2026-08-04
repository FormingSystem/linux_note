#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "protocol/framing.h"
#include "protocol/messages.h"

#include <uv.h>

namespace {

constexpr std::size_t k_maximum_pending_writes = 64U;
constexpr std::size_t k_maximum_pending_write_bytes = 8U * 1024U * 1024U;
static_assert(
    k_maximum_pending_write_bytes
        >= loop::protocol::k_frame_header_bytes
            + loop::protocol::k_max_control_frame_bytes
            + loop::protocol::k_max_body_frame_bytes,
    "native write queue must accept one maximum-size frame");

class stdio_server {
 public:
  int run() {
    loop_ = uv_default_loop();
    if (uv_pipe_init(loop_, &input_, 0) != 0 || uv_pipe_init(loop_, &output_, 0) != 0) {
      return EXIT_FAILURE;
    }
    input_.data = this;
    output_.data = this;
    if (uv_pipe_open(&input_, 0) != 0 || uv_pipe_open(&output_, 1) != 0) {
      return EXIT_FAILURE;
    }
    const auto started = uv_read_start(
        reinterpret_cast<uv_stream_t*>(&input_), allocate, read);
    if (started != 0) return EXIT_FAILURE;
    uv_run(loop_, UV_RUN_DEFAULT);
    return exit_code_;
  }

 private:
  struct pending_write {
    uv_write_t request{};
    std::vector<unsigned char> bytes;
    stdio_server* owner = nullptr;
  };

  static void allocate(uv_handle_t*, const std::size_t, uv_buf_t* buffer) {
    constexpr std::size_t size = 64U * 1024U;
    buffer->base = new char[size];
    buffer->len = static_cast<decltype(buffer->len)>(size);
  }

  static void read(uv_stream_t* stream, const std::ptrdiff_t count, const uv_buf_t* buffer) {
    std::unique_ptr<char[]> storage(buffer->base);
    auto* self = static_cast<stdio_server*>(stream->data);
    if (count > 0) {
      self->on_bytes(std::span(
          reinterpret_cast<const unsigned char*>(buffer->base),
          static_cast<std::size_t>(count)));
      return;
    }
    if (count == UV_EOF && self->decoder_.finish()) {
      self->finish(EXIT_SUCCESS);
      return;
    }
    if (count < 0) self->finish(EXIT_FAILURE);
  }

  void on_bytes(const std::span<const unsigned char> bytes) {
    try {
      for (const auto& request : decoder_.push(bytes)) {
        if (!queue_write(loop::protocol::encode_frame(request_handler_.handle_request(request)))) {
          finish(EXIT_FAILURE);
          return;
        }
      }
    } catch (...) {
      finish(EXIT_FAILURE);
    }
  }

  bool queue_write(std::vector<unsigned char> bytes) {
    if (pending_writes_ >= k_maximum_pending_writes
        || bytes.size() > k_maximum_pending_write_bytes - pending_write_bytes_) {
      return false;
    }
    auto* write = new pending_write;
    write->bytes = std::move(bytes);
    write->owner = this;
    write->request.data = write;
    uv_buf_t buffer = uv_buf_init(
        reinterpret_cast<char*>(write->bytes.data()),
        static_cast<unsigned int>(write->bytes.size()));
    ++pending_writes_;
    pending_write_bytes_ += write->bytes.size();
    const auto result = uv_write(
        &write->request,
        reinterpret_cast<uv_stream_t*>(&output_),
        &buffer,
        1,
        write_complete);
    if (result != 0) {
      --pending_writes_;
      pending_write_bytes_ -= write->bytes.size();
      delete write;
      finish(EXIT_FAILURE);
      return false;
    }
    return true;
  }

  static void write_complete(uv_write_t* request, const int status) {
    auto* write = static_cast<pending_write*>(request->data);
    auto* self = write->owner;
    const auto byte_count = write->bytes.size();
    delete write;
    --self->pending_writes_;
    self->pending_write_bytes_ -= byte_count;
    if (status != 0) {
      self->finish(EXIT_FAILURE);
    } else if (self->finishing_ && self->pending_writes_ == 0U) {
      uv_stop(self->loop_);
    }
  }

  void finish(const int exit_code) {
    if (finishing_) {
      if (exit_code != EXIT_SUCCESS) exit_code_ = EXIT_FAILURE;
      return;
    }
    finishing_ = true;
    exit_code_ = exit_code;
    uv_read_stop(reinterpret_cast<uv_stream_t*>(&input_));
    if (pending_writes_ == 0U) uv_stop(loop_);
  }

  uv_loop_t* loop_ = nullptr;
  uv_pipe_t input_{};
  uv_pipe_t output_{};
  loop::protocol::frame_decoder decoder_;
  loop::protocol::request_handler request_handler_;
  std::size_t pending_writes_ = 0U;
  std::size_t pending_write_bytes_ = 0U;
  bool finishing_ = false;
  int exit_code_ = EXIT_SUCCESS;
};

}  // namespace

int main(const int argc, const char* const argv[]) {
  if (argc != 2 || std::string_view(argv[1]) != "--stdio") return EXIT_FAILURE;
  stdio_server server;
  return server.run();
}
