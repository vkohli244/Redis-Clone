#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

class Database {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    std::size_t del(const std::string& key);

private:
    std::unordered_map<std::string, std::string> data_;
};
