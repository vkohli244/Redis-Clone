#include "database.h"

#include <utility>

void Database::set(const std::string &key, std::string value,
                   std::optional<std::chrono::milliseconds> duration) {

  std::optional<std::chrono::steady_clock::time_point> expiry;

  if (duration) { // no duration passed to the function means that no EX or PX argument
    expiry = std::chrono::steady_clock::now() + *duration;
  }

  data_.insert_or_assign(key, Entry{std::move(value), expiry});
}

std::optional<std::string> Database::get(const std::string &key) {
    auto search = data_.find(key);      // find key , returns iterable object

    if (search == data_.end()) {        // if the iterable is the end that means the key doesn't exist so return nullopt
        return std::nullopt;
    }

    const auto &entry = search->second; // get the entry object

    if (entry.expiry) {                 // if the entry object contains an expiry time
        const auto now = std::chrono::steady_clock::now();

        if (now >= *entry.expiry) {     // check if the expiry time has passed
            return std::nullopt;
        }
    }

    return entry.value;                 // otherwise return
}

std::size_t Database::del(const std::string &key) {

  if (auto search = data_.find(key); search != data_.end()) {
    return data_.erase(key);
  }

  return 0;
}
