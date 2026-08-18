#include "connection.h"

#include "command_handler.h"
#include "parser.h"

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <sys/socket.h>

namespace { // private functions for this file only
void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

void msg_errno(const char *msg) {
  fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

void bufappend(std::vector<std::uint8_t> &buf, std::string_view data) {
  buf.insert(buf.end(), data.begin(), data.end());
}

void bufconsume(std::vector<std::uint8_t> &buf, std::size_t count) {
  assert(count <= buf.size());
  using Difference = std::vector<std::uint8_t>::difference_type;
  buf.erase(buf.begin(), buf.begin() + static_cast<Difference>(count));
}

ssize_t send_without_sigpipe(int fd, const void *buffer, std::size_t size) {
#ifdef MSG_NOSIGNAL
  return send(fd, buffer, size, MSG_NOSIGNAL);
#else
  return send(fd, buffer, size, 0);
#endif
}
} // namespace

Connection::Connection(int fd) : fd(fd), want_read(true) {}

void Connection::handle_write() {
  while (!outgoing.empty()) {
    const ssize_t rv =
        send_without_sigpipe(fd, outgoing.data(), outgoing.size());

    if (rv > 0) {
      bufconsume(outgoing, static_cast<std::size_t>(rv));
      continue;
    }

    if (rv < 0 && errno == EINTR) {
      continue;
    }

    if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }

    msg_errno("send error");
    want_close = true;
    return;
  }
  want_write = false;
  want_read = !close_after_write_;
  want_close = close_after_write_;
  return;
}

void Connection::handle_read(CommandHandler &command_handler) {
  char buffer[64 * 1024];

  while (true) {
    const ssize_t bytes_received = recv(fd, buffer, sizeof(buffer), 0);

    if (bytes_received > 0) {
      bufappend(
          incoming,
          std::string_view{buffer, static_cast<std::size_t>(bytes_received)});
      continue;
    }

    if (bytes_received == 0) {
      handle_request(command_handler);
      if (outgoing.empty()) {
        want_close = true;
      } else {
        close_after_write_ = true;
        want_read = false;
        want_write = true;
      }
      return;
    }

    if (errno == EINTR) {
      continue;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    }

    msg_errno("recv error");
    want_close = true;
    return;
  }

  handle_request(command_handler);
}

void Connection::handle_request(CommandHandler &command_handler) {
  while (true) {
    ParseResult result = parse_command(incoming);

    if (result.status == ParseStatus::Incomplete) {
      return;
    }

    if (result.status == ParseStatus::Invalid) {
      msg("Invalid RESP request");
      want_close = true;
      return;
    }

    std::string response = command_handler.execute(result.args);
    bufconsume(incoming, result.bytes_consumed);

    bufappend(outgoing, response);

    want_read = false;
    want_write = true;
  }
}
