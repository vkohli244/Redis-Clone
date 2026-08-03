#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ParseStatus {
    Complete,
    Incomplete,
    Invalid
};

struct ParseResult {
    ParseStatus status = ParseStatus::Incomplete;
    std::vector<std::string> args;
    std::size_t bytes_consumed = 0;
};

std::size_t find_crlf(
    const std::vector<std::uint8_t>& buf,
    std::size_t start
);

ParseResult small_parse(
    const std::vector<std::uint8_t>& buf
);
