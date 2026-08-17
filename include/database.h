#pragma once
#ifndef DATABASE_H
#define DATABASE_H
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

class Database {
public:
  void set(const std::string &key, std::string value,
           std::optional<std::chrono::milliseconds> duration = std::nullopt);

  std::optional<std::string> get(const std::string &key);
  std::size_t del(const std::string &key);

private:
  struct Entry {
    std::string value;
    std::optional<std::chrono::steady_clock::time_point> expiry;
  };

  std::unordered_map<std::string, Entry> data_;
};

#endif
