#pragma once

#ifndef SERVER_H
#define SERVER_H

#include "command_handler.h"
#include "connection.h"
#include "database.h"

#include <poll.h>
#include <vector>

class RedisServer {
public:
  RedisServer();

  int run();

private:
  int server_fd_ = -1;
  std::vector<struct pollfd> fds_;
  std::vector<Connection *> fd2conn_;
  Database database_;
  CommandHandler command_handler_;
};

#endif
