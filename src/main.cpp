#include "server.h"

#include <iostream>

int main() {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  RedisServer server;
  return server.run();
}
