#pragma once

#ifndef SERVER_H
#define SERVER_H

#include "command_handler.h"
#include "connection.h"
#include "database.h"

#include <poll.h>
#include <atomic>
#include <cstdint>
#include <vector>

class RedisServer {
public:
  explicit RedisServer(std::uint16_t port = 6379);
  ~RedisServer();

  RedisServer(const RedisServer &) = delete;
  RedisServer &operator=(const RedisServer &) = delete;

  int run();
  void request_stop();
  std::uint16_t port() const;

private:
  void close_all();

  std::uint16_t requested_port_;
  std::atomic<std::uint16_t> bound_port_{0};
  std::atomic<bool> stop_requested_{false};
  int server_fd_ = -1;
  std::vector<struct pollfd> fds_;
  std::vector<Connection *> fd2conn_;
  Database database_;
  CommandHandler command_handler_;
};

#endif
