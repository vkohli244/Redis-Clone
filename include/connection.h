#pragma once
#ifndef CONNECTION_H
#define CONNECTION_H
#include <cstdint>
#include <vector>

class CommandHandler;

class Connection {
public:
  explicit Connection(int fd);

  void handle_read(CommandHandler &command_handler);
  void handle_request(CommandHandler &command_handler);
  void handle_write();

  int fd = -1;

  bool want_read = false;
  bool want_close = false;
  bool want_write = false;
  std::vector<std::uint8_t> incoming;
  std::vector<std::uint8_t> outgoing;
};
#endif
