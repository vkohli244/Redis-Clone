#include "server.h"

#include <arpa/inet.h>
#include <assert.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
void msg_errno(const char *msg) {
  fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

bool set_nonblocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool configure_client(int fd) {
  if (!set_nonblocking(fd)) {
    return false;
  }
#ifdef SO_NOSIGPIPE
  int no_sigpipe = 1;
  return setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                    sizeof(no_sigpipe)) == 0;
#else
  return true;
#endif
}
} // namespace

RedisServer::RedisServer(std::uint16_t port)
    : requested_port_(port), command_handler_(database_) {}

RedisServer::~RedisServer() { close_all(); }

void RedisServer::request_stop() { stop_requested_.store(true); }

std::uint16_t RedisServer::port() const { return bound_port_.load(); }

void RedisServer::close_all() {
  for (Connection *conn : fd2conn_) {
    if (conn) {
      close(conn->fd);
      delete conn;
    }
  }
  fd2conn_.clear();

  if (server_fd_ >= 0) {
    close(server_fd_);
    server_fd_ = -1;
  }
  bound_port_.store(0);
}

int RedisServer::run() {
  stop_requested_.store(false);
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  int reuse = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) !=
          0 ||
      !set_nonblocking(server_fd_)) {
    std::cerr << "listener configuration failed\n";
    close_all();
    return 1;
  }

  struct sockaddr_in server_address = {};
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(requested_port_);
  if (bind(server_fd_, reinterpret_cast<struct sockaddr *>(&server_address),
           sizeof(server_address)) != 0) {
    std::cerr << "Failed to bind to port " << requested_port_ << "\n";
    close_all();
    return 1;
  }

  if (listen(server_fd_, SOMAXCONN) != 0) {
    std::cerr << "listen failed\n";
    close_all();
    return 1;
  }

  struct sockaddr_in bound_address = {};
  socklen_t bound_address_length = sizeof(bound_address);
  if (getsockname(server_fd_,
                  reinterpret_cast<struct sockaddr *>(&bound_address),
                  &bound_address_length) != 0) {
    std::cerr << "getsockname failed\n";
    close_all();
    return 1;
  }
  bound_port_.store(ntohs(bound_address.sin_port));

  while (!stop_requested_.load()) {
    fds_.clear(); // clear the fds list on every iteration

    struct pollfd listener = {
        server_fd_, POLLIN,
        0}; // rebuild the listening socket pollfd struct on each iteration
    fds_.push_back(listener);

    for (Connection *conn : fd2conn_) {
      if (!conn) { // if there is no conn struct in this index
        continue;
      }
      struct pollfd pfd = {conn->fd, 0,
                           0}; // currently POLLIN since we have no bool flags

      if (conn->want_read) {
        pfd.events |= POLLIN;
      }

      if (conn->want_write) {
        pfd.events |= POLLOUT;
      }

      fds_.push_back(pfd);
    }

    const auto poll_count = static_cast<nfds_t>(fds_.size());
    int rv = poll(fds_.data(), poll_count, 25);

    if (rv < 0 && errno == EINTR) {
      continue; // No error occured build the polled fds vector again
    }

    if (rv < 0) {
      msg_errno("poll error");
      close_all();
      return 1;
    }

    if (rv == 0) {
      continue;
    }

    // Check the listening socket (index 0) for a new connection
    if (fds_[0].revents & POLLIN) {
      while (true) {
        struct sockaddr_in client_address = {};
        socklen_t client_address_length = sizeof(client_address);
        const int client_fd =
            accept(server_fd_,
                   reinterpret_cast<struct sockaddr *>(&client_address),
                   &client_address_length);
        if (client_fd < 0) {
          if (errno != EAGAIN && errno != EWOULDBLOCK) {
            msg_errno("accept() error");
          }
          break;
        }
        if (!configure_client(client_fd)) {
          close(client_fd);
          continue;
        }

        Connection *conn = new Connection(client_fd);
        const auto connection_index = static_cast<std::size_t>(conn->fd);
        if (fd2conn_.size() <= connection_index) {
          fd2conn_.resize(connection_index + 1);
        }

        fd2conn_[connection_index] = conn;
      }
    }

    // Check every client (index 1 onward) for incoming data
    for (size_t i = 1; i < fds_.size(); ++i) { // note: skip the 1st
      const short ready = fds_[i].revents;

      if (ready == 0) {
        continue;
      }

      const auto connection_index =
          static_cast<std::size_t>(fds_[i].fd);
      Connection *conn = fd2conn_[connection_index];

      if (ready & POLLIN) {
        assert(conn->want_read);
        conn->handle_read(command_handler_);
      }

      if (ready & POLLOUT) {
        assert(conn->want_write);
        conn->handle_write();
      }

      const bool fatal_event = (ready & (POLLERR | POLLNVAL)) != 0;
      const bool finished_hangup =
          (ready & POLLHUP) != 0 && !conn->want_write;
      if (fatal_event || finished_hangup || conn->want_close) {
        fd2conn_[connection_index] = nullptr;
        close(fds_[i].fd);
        delete conn;
      }
    }
  }
  close_all();
  return 0;
}
