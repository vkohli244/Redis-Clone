
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "database.h"

#include <string>
#include <vector>

class CommandHandler {
public:
  explicit CommandHandler(Database &db);

  std::string execute(const std::vector<std::string> &args);

private:
  Database &db_;
  std::string handle_del(const std::vector<std::string> &args);
  std::string handle_get(const std::vector<std::string> &args);
  std::string handle_set(const std::vector<std::string> &args);
};
#endif
