#include "command_handler.h"

#define to_MS 1000
CommandHandler::CommandHandler(Database &db) : db_(db) {}

std::string CommandHandler::execute(const std::vector<std::string> &args) {
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
      const std::string &message = args[1];

      response =
          "$" + std::to_string(message.size()) + "\r\n" + message + "\r\n";
    }
  } else if (command == "SET") {
    return handle_set(args);
  }
  else if (command == "GET"){
      // implement handle_get()
  }
  else{
      return "-ERR unknown command\r\n";
  }

  return response;
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
  const std::string &option = args[3];
  const std::string &expiry_arg = args[4];
  int duration{};

  // check if entire expiry_arg is a string
  for (char ch : expiry_arg) {
    if (!isdigit(ch)) { // consider switching to static_cast<unsigned char>
                        // (ch) for portability
      return "-ERR Invalid argument for expiry duration \r\n";
    }
  }

  // convert expiry_arg to duration
  try {
    duration = std::stoi(expiry_arg);
  } catch (std::invalid_argument const &ex) {
    return "-ERR Invalid argument for expiry duation\r\n";
  } catch (std::out_of_range const &ex) {
    return "-ERR duration out of range\r\n";
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
