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


std::size_t find_crlf(const std::vector<uint8_t> &buf, size_t start);

ParseResult small_parse(const std::vector<uint8_t> &buf);
