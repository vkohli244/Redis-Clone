#include "server.h"

#include <arpa/inet.h>
#include <assert.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace {
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

Connection *handle_accept(int fd) {

  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  int client_fd =
      accept(fd, reinterpret_cast<struct sockaddr *>(&client_addr),
             &client_addr_len);

  if (client_fd < 0) {
    msg_errno("accept() error");
    return nullptr;
  }

  fd_set_nb(client_fd);

  Connection *conn = new Connection(client_fd);
  return conn;
}
} // namespace

RedisServer::RedisServer() : command_handler_(database_) {}

int RedisServer::run() {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);

  if (bind(server_fd_, reinterpret_cast<struct sockaddr *>(&server_addr),
           sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd_, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  while (true) {
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

    if (!std::in_range<nfds_t>(fds_.size())) {
      die("too many file descriptors to poll");
    }

    const auto poll_count = static_cast<nfds_t>(fds_.size());
    int rv = poll(fds_.data(), poll_count, -1);

    if (rv < 0 && errno == EINTR) {
      continue; // No error occured build the polled fds vector again
    }

    if (rv < 0) {
      die("poll");
    }

    // Check the listening socket (index 0) for a new connection
    if (fds_[0].revents & POLLIN) {
      if (Connection *conn = handle_accept(server_fd_)) {
        std::cout << "Client Connected";
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

      if (ready & POLLERR || conn->want_close) {
        std::cout << "Client closed\n";
        fd2conn_[connection_index] = nullptr;
        close(fds_[i].fd);
        delete conn;
      }
    }
  }
  close(server_fd_);
  return 0;
}
