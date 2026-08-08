#include "../include/command_handler.h"

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

      response = "$" + std::to_string(message.size())
                 + "\r\n" + message + "\r\n";
    }
  } else {
    response = "-ERR unknown command\r\n";
  }

  return response;
}
