#include "connection.h"

#include "command_handler.h"
#include "parser.h"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <string>
#include <sys/socket.h>

namespace {
void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

void msg_errno(const char *msg) {
  fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

void bufappend(std::vector<std::uint8_t> &buf, const char *data,
               std::size_t len) {
  buf.insert(buf.end(), data, data + len);
}

void bufconsume(std::vector<std::uint8_t> &buf, std::size_t count) {
  buf.erase(buf.begin(), buf.begin() + count);
}
} // namespace

Connection::Connection(int fd) : fd(fd), want_read(true) {}

void Connection::handle_write() {
  while (!outgoing.empty()) {
    const ssize_t rv = send(fd, outgoing.data(), outgoing.size(), 0);

    if (rv > 0) {
      bufconsume(outgoing, static_cast<std::size_t>(rv));
      continue;
    }

    if (rv < 0 && errno == EINTR) {
      continue;
    }

    if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      msg("socket not currently writable");
      return;
    }

    msg_errno("send error");
    want_close = true;
    return;
  }
  want_write = false;
  want_read = true;
  return;
}

void Connection::handle_read(CommandHandler &command_handler) {
  char buffer[64 * 1024];

  while (true) {
    const ssize_t bytes_received = recv(fd, buffer, sizeof(buffer), 0);

    if (bytes_received > 0) {
      bufappend(incoming, buffer, static_cast<std::size_t>(bytes_received));
      continue;
    }

    if (bytes_received == 0) {
      want_close = true;
      msg("eof");
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

    if (result.args.empty()) {
      msg("No arguments present");
      want_close = true;
      return;
    }

    std::string response = command_handler.execute(result.args);
    bufconsume(incoming, result.bytes_consumed);

    bufappend(outgoing, response.data(), response.size());

    want_read = false;
    want_write = true;
    return;
  }
}
