#pragma once

#include "database.h"

#include <string>
#include <vector>

class CommandHandler {
public:
  explicit CommandHandler(Database &db);

  std::string execute(const std::vector<std::string> &args);

private:
  Database &db_;
};
