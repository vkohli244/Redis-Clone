#include "database.h"

void Database::set(const std::string& key, const std::string& value){
    data_.insert_or_assign(key, value);
    // don't store the value of this because returns true if insert, false if assigned both mean true in
    // our application

}

std::optional<std::string> Database::get(const std::string& key) const {
    if (auto search = data_.find(key); search != data_.end()){
        return search->second;
    }

    return std::nullopt;
}

std::size_t Database::del(const std::string& key){

    if (auto search = data_.find(key); search != data_.end()){
        return data_.erase(key);
    }

    return 0;

}
