#include "command_handler.h"

#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#define to_MS 1000
CommandHandler::CommandHandler(Database &db) : db_(db) {}

std::string CommandHandler::execute(const std::vector<std::string> &args) {
  if (args.empty()) {
    return "-ERR empty command\r\n";
  }

  const std::string &command = args[0];
  std::string response;

  if (command == "PING") {

    if (args.size() == 1) {
      response = "+PONG\r\n";

    } else {
      response = "-ERR wrong number of arguments for "
                 "'PING' command\r\n";
    }

  } else if (command == "ECHO") {

    if (args.size() != 2) {
      response = "-ERR wrong number of arguments for "
                 "'ECHO' command\r\n";
    } else {
      const std::string &message = args[1]; // change to use modern c++ string_view

      response =
          "$" + std::to_string(message.size()) + "\r\n" + message + "\r\n";
    }
  } else if (command == "SET") {
    return handle_set(args);
  } else if (command == "GET") {
    return handle_get(args);
  } else if (command == "DEL") {
    return handle_del(args);
  } else {
    return "-ERR unknown command\r\n";
  }

  return response;
}

std::string CommandHandler::handle_del(const std::vector<std::string> &args) {
  if (args.size() < 2) {
    return "-ERR wrong number of arguments for 'DEL' command\r\n";
  }

  std::size_t deleted_count = 0;

  for (std::size_t index = 1; index < args.size(); ++index) {
    deleted_count += db_.del(args[index]);
  }

  return ":" + std::to_string(deleted_count) + "\r\n";
}

std::string CommandHandler::handle_get(const std::vector<std::string> &args) {
  if (args.size() != 2) {
    return "-ERR wrong number of arguments for 'GET' command\r\n";
  }

  const auto value = db_.get(args[1]);

  if (!value) {
    return "$-1\r\n";
  }

  return "$" + std::to_string(value->size()) + "\r\n" + *value + "\r\n";
}

std::string CommandHandler::handle_set(const std::vector<std::string> &args) {

  if (args.size() != 3 && args.size() != 5) {
    return "-ERR wrong number of arguments for 'SET' command\r\n";
  }

  const std::string &key = args[1];
  const std::string &value = args[2];

  // Plain SET
  if (args.size() == 3) {
    db_.set(key, value);
    return "+OK\r\n";
  }

  // SET with expiry option
  const std::string_view option = args[3];
  const std::string_view expiry_arg = args[4];
  int duration{};

  const char *const expiry_end = expiry_arg.data() + expiry_arg.size();
  const auto [parse_end, error] =
      std::from_chars(expiry_arg.data(), expiry_end, duration);

  if (error == std::errc::result_out_of_range) {
    return "-ERR duration out of range\r\n";
  }

  if (error != std::errc{} || parse_end != expiry_end) {
    return "-ERR Invalid argument for expiry duration \r\n";
  }

  if (duration <= 0) {
    return "-ERR invalid expire time in 'SET' command\r\n";
  }

  if (option == "PX") {
    const auto int_ms = std::chrono::milliseconds(duration);

    db_.set(key, value, int_ms);

    return "+OK\r\n";
  }

  if (option == "EX") {
    const auto int_ms = std::chrono::
                        duration_cast<std::chrono::milliseconds>
                        (std::chrono::seconds(duration));

    db_.set(key, value, int_ms); // set takes milliseconds
    return "+OK\r\n";
  }

  return "-ERR syntax error\r\n";
}
