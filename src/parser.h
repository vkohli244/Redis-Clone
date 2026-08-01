#pragma once
#include <vector>
#include <string>
#include <cstddef>



enum class ParseStatus{
    Complete,
    Incomplete,
    Invalid
};


struct ParseResult{
    ParseStatus status = ParseStatus::Incomplete;
    std::vector<std::string> args;
    std::size_t bytes_consumed = 0;
};
