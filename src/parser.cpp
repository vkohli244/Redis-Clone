#include "parser.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

static const std::size_t NPOS = static_cast<std::size_t>(-1);

std::size_t find_crlf(
    const std::vector<uint8_t>& buf,
    std::size_t start
) {
    for (std::size_t i = start; i + 1 < buf.size(); ++i) {
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            return i;
        }
    }

    return NPOS;
}

ParseResult small_parse(const std::vector<uint8_t>& buf) {
    ParseResult result;

    if (buf.empty()) {
        result.status = ParseStatus::Incomplete;
        return result;
    }

    const std::size_t crlf_index = find_crlf(buf, 0);

    if (crlf_index == NPOS) {
        result.status = ParseStatus::Incomplete;
        return result;
    }

    if (buf[0] != '*') {
        result.status = ParseStatus::Invalid;
        return result;
    }

    // There must be at least one digit between '*' and CRLF.
    if (crlf_index == 1) {
        result.status = ParseStatus::Invalid;
        return result;
    }

    std::size_t num_elements = 0;

    for (std::size_t i = 1; i < crlf_index; ++i) {
        if (buf[i] < '0' || buf[i] > '9') {
            result.status = ParseStatus::Invalid;
            return result;
        }

        const std::size_t digit =
            static_cast<std::size_t>(buf[i] - '0');

        if (
            num_elements >
            (std::numeric_limits<std::size_t>::max() - digit) / 10
        ) {
            result.status = ParseStatus::Invalid;
            return result;
        }

        num_elements = num_elements * 10 + digit;
    }

    // A Redis command must contain at least one element:
    // the command name.
    if (num_elements == 0) {
        result.status = ParseStatus::Invalid;
        return result;
    }

    // Move past the array header.
    std::size_t pos = crlf_index + 2;

    for (
        std::size_t element_index = 0;
        element_index < num_elements;
        ++element_index
    ) {
        // The array expects another element, but it has not arrived yet.
        if (pos >= buf.size()) {
            result.status = ParseStatus::Incomplete;
            return result;
        }

        if (buf[pos] != '$') {
            result.status = ParseStatus::Invalid;
            return result;
        }

        const std::size_t length_crlf_index =
            find_crlf(buf, pos);

        if (length_crlf_index == NPOS) {
            result.status = ParseStatus::Incomplete;
            return result;
        }

        // There must be at least one character between '$' and CRLF.
        if (length_crlf_index == pos + 1) {
            result.status = ParseStatus::Invalid;
            return result;
        }

        std::size_t string_length = 0;

        for (
            std::size_t i = pos + 1;
            i < length_crlf_index;
            ++i
        ) {
            if (buf[i] < '0' || buf[i] > '9') {
                result.status = ParseStatus::Invalid;
                return result;
            }

            const std::size_t digit =
                static_cast<std::size_t>(buf[i] - '0');

            if (
                string_length >
                (std::numeric_limits<std::size_t>::max() - digit) / 10
            ) {
                result.status = ParseStatus::Invalid;
                return result;
            }

            string_length = string_length * 10 + digit;
        }

        // Move past "$<length>\r\n".
        const std::size_t string_start =
            length_crlf_index + 2;

        // Prevent overflow in string_start + string_length.
        if (
            string_length >
            std::numeric_limits<std::size_t>::max() - string_start
        ) {
            result.status = ParseStatus::Invalid;
            return result;
        }

        const std::size_t string_end =
            string_start + string_length;

        // Check whether the complete payload and trailing CRLF exist.
        if (
            string_end > buf.size() ||
            buf.size() - string_end < 2
        ) {
            result.status = ParseStatus::Incomplete;
            return result;
        }

        if (
            buf[string_end] != '\r' ||
            buf[string_end + 1] != '\n'
        ) {
            result.status = ParseStatus::Invalid;
            return result;
        }

        result.args.emplace_back(
            reinterpret_cast<const char*>(&buf[string_start]),
            string_length
        );

        // Move past the payload and its trailing CRLF so the next loop
        // iteration starts at the next bulk string.
        pos = string_end + 2;
    }

    result.status = ParseStatus::Complete;
    result.bytes_consumed = pos;

    return result;
}
