
#include "utils.h"
#include "parser.h"
#include <cstring>
#include <string>
#include <sys/socket.h>

void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

void msg_errno(const char *msg) {
  fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

void die(const char *msg) {
  fprintf(stderr, "[%d] %s\n", errno, msg);
  abort();
}

void fd_set_nb(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);

  if (flags == -1) {
    die("fcntl error");
    return;
  }

  flags |= O_NONBLOCK;

  int rv = fcntl(fd, F_SETFL, flags);
  if (rv < 0) {
    die("fcntl error");
  }
}

Conn *handle_accept(int fd) {

  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  int client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_addr_len);

  if (client_fd < 0) {
    msg_errno("accept() error");
    return NULL;
  }

  fd_set_nb(client_fd);

  Conn *conn = new Conn;
  conn->fd = client_fd;
  conn->want_read = true;
  return conn;
}

void bufappend(std::vector<uint8_t> &buf, const char *data, size_t len) {
  buf.insert(buf.end(), data, data + len);
}

void bufconsume(std::vector<uint8_t> &buf, std::size_t count) {
  buf.erase(buf.begin(), buf.begin() + count);
}

void handle_write(Conn *conn) {
  while (!conn->outgoing.empty()) {
    const ssize_t rv =
        send(conn->fd, conn->outgoing.data(), conn->outgoing.size(), 0);

    if (rv > 0) {
      bufconsume(conn->outgoing, static_cast<std::size_t>(rv));
      continue;
    }

    if (rv < 0 && errno == EINTR) {
      continue;
    }

    if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      msg("socket not currently writable");
      return false;
    }

    msg_errno("send error");
    conn->want_close = true;
    return false;
  }
  conn->want_write = false;
  conn->want_read = true;
  return true;
}

void handle_read(Conn *conn) {
  char buffer[64 * 1024];

  while (true) {
    const ssize_t bytes_received = recv(conn->fd, buffer, sizeof(buffer), 0);

    if (bytes_received > 0) {
      bufappend(conn->incoming, buffer,
                static_cast<std::size_t>(bytes_received));
      continue;
    }

    if (bytes_received == 0) {
      conn->want_close = true;
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
    conn->want_close = true;
    return;
  }

  handle_request(conn);
}

void handle_request(Conn *conn) {
  while (true) {
    ParseResult result = small_parse(conn->incoming);

    if (result.status == ParseStatus::Incomplete) {
      return;
    }

    if (result.status == ParseStatus::Invalid) {
      msg("Invalid RESP request");
      conn->want_close = true;
      return;
    }

    if (result.args.empty()) {
      msg("No arguments present");
      conn->want_close = true;
      return;
    }

    const std::string &command = result.args[0];
    std::string response;

    if (command == "PING") {
      if (result.args.size() == 1) {
        response = "+PONG\r\n";
      } else {
        response = "-ERR wrong number of arguments for "
                   "'PING' command\r\n";
      }
    } else if (command == "ECHO") {
      if (result.args.size() != 2) {
        response = "-ERR wrong number of arguments for "
                   "'ECHO' command\r\n";
      } else {
        const std::string &message = result.args[1];

        response =
            "$" + std::to_string(message.size()) + "\r\n" + message + "\r\n";
      }
    } else {
      response = "-ERR unknown command\r\n";
    }
    bufconsume(conn->incoming, result.bytes_consumed);

    bufappend(conn->outgoing, response.data(), response.size());

    conn->want_read = false;
    conn->want_write = true;
    return;
  }
}
