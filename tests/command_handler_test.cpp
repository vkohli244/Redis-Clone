#include "command_handler.h"
#include "database.h"
#include "test_support.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(command_handler_covers_ping_echo_and_unknown_commands) {
  Database database;
  CommandHandler handler(database);

  REQUIRE_EQ(handler.execute({}), "-ERR empty command\r\n");
  REQUIRE_EQ(handler.execute({"PING"}), "+PONG\r\n");
  REQUIRE_EQ(handler.execute({"PING", "extra"}),
             "-ERR wrong number of arguments for 'PING' command\r\n");
  REQUIRE_EQ(handler.execute({"ECHO"}),
             "-ERR wrong number of arguments for 'ECHO' command\r\n");
  REQUIRE_EQ(handler.execute({"ECHO", "hello"}), "$5\r\nhello\r\n");
  REQUIRE_EQ(handler.execute({"ECHO", "one", "two"}),
             "-ERR wrong number of arguments for 'ECHO' command\r\n");
  REQUIRE_EQ(handler.execute({"NOPE"}), "-ERR unknown command\r\n");
}

TEST(command_handler_covers_set_get_and_del_arity) {
  Database database;
  CommandHandler handler(database);

  REQUIRE_EQ(handler.execute({"SET"}),
             "-ERR wrong number of arguments for 'SET' command\r\n");
  REQUIRE_EQ(handler.execute({"SET", "key", "value", "PX"}),
             "-ERR wrong number of arguments for 'SET' command\r\n");
  REQUIRE_EQ(handler.execute({"SET", "key", "value"}), "+OK\r\n");
  REQUIRE_EQ(handler.execute({"GET"}),
             "-ERR wrong number of arguments for 'GET' command\r\n");
  REQUIRE_EQ(handler.execute({"GET", "key", "extra"}),
             "-ERR wrong number of arguments for 'GET' command\r\n");
  REQUIRE_EQ(handler.execute({"GET", "key"}), "$5\r\nvalue\r\n");
  REQUIRE_EQ(handler.execute({"GET", "missing"}), "$-1\r\n");
  REQUIRE_EQ(handler.execute({"DEL"}),
             "-ERR wrong number of arguments for 'DEL' command\r\n");
  REQUIRE_EQ(handler.execute({"DEL", "key", "missing"}), ":1\r\n");
  REQUIRE_EQ(handler.execute({"DEL", "key"}), ":0\r\n");
}

TEST(command_handler_validates_every_expiry_form) {
  Database database;
  CommandHandler handler(database);

  REQUIRE_EQ(handler.execute({"SET", "px", "value", "PX", "20"}),
             "+OK\r\n");
  REQUIRE_EQ(handler.execute({"GET", "px"}), "$5\r\nvalue\r\n");
  REQUIRE_EQ(handler.execute({"SET", "ex", "value", "EX", "1"}),
             "+OK\r\n");
  REQUIRE_EQ(handler.execute({"GET", "ex"}), "$5\r\nvalue\r\n");
  REQUIRE_EQ(handler.execute({"SET", "key", "value", "NX", "1"}),
             "-ERR syntax error\r\n");
  REQUIRE_EQ(handler.execute({"SET", "key", "value", "PX", "0"}),
             "-ERR invalid expire time in 'SET' command\r\n");
  REQUIRE_EQ(handler.execute({"SET", "key", "value", "PX", "-1"}),
             "-ERR invalid expire time in 'SET' command\r\n");
  REQUIRE_EQ(handler.execute({"SET", "key", "value", "PX", "abc"}),
             "-ERR Invalid argument for expiry duration \r\n");
  REQUIRE_EQ(handler.execute({"SET", "key", "value", "PX", "1ms"}),
             "-ERR Invalid argument for expiry duration \r\n");
  REQUIRE_EQ(handler.execute(
                 {"SET", "key", "value", "PX", "999999999999999999999"}),
             "-ERR duration out of range\r\n");

  std::this_thread::sleep_for(25ms);
  REQUIRE_EQ(handler.execute({"GET", "px"}), "$-1\r\n");
}
