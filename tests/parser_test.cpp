#include "parser.h"
#include "test_support.h"

#include <limits>
#include <string>

TEST(find_crlf_honors_start_and_missing_delimiters) {
  const auto input = bytes("a\r\nb\r\n");
  REQUIRE_EQ(find_crlf(input, 0), 1U);
  REQUIRE_EQ(find_crlf(input, 2), 4U);
  REQUIRE_EQ(find_crlf(input, input.size()),
             std::numeric_limits<std::size_t>::max());
  REQUIRE_EQ(find_crlf(bytes("\rX\n"), 0),
             std::numeric_limits<std::size_t>::max());
}

TEST(parser_reports_incomplete_at_every_valid_fragment_boundary) {
  const std::string command = resp({"SET", "alpha", "bravo"});
  for (std::size_t length = 0; length < command.size(); ++length) {
    const ParseResult result =
        parse_command(bytes(std::string_view(command).substr(0, length)));
    REQUIRE_EQ(result.status, ParseStatus::Incomplete);
    REQUIRE_EQ(result.bytes_consumed, 0U);
  }

  const ParseResult complete = parse_command(bytes(command));
  REQUIRE_EQ(complete.status, ParseStatus::Complete);
  REQUIRE_EQ(complete.args, std::vector<std::string>({"SET", "alpha", "bravo"}));
  REQUIRE_EQ(complete.bytes_consumed, command.size());
}

TEST(parser_rejects_malformed_array_headers) {
  const std::vector<std::string> invalid = {
      "+1\r\n", "*\r\n", "*-1\r\n", "*1x\r\n", "*0\r\n",
      "*184467440737095516160\r\n"};
  for (const std::string &input : invalid) {
    REQUIRE_EQ(parse_command(bytes(input)).status, ParseStatus::Invalid);
  }
}

TEST(parser_rejects_malformed_bulk_strings) {
  const std::vector<std::string> invalid = {
      "*1\r\n+3\r\nGET\r\n",
      "*1\r\n$\r\n",
      "*1\r\n$-1\r\n",
      "*1\r\n$1x\r\na\r\n",
      "*1\r\n$184467440737095516160\r\n",
      "*1\r\n$18446744073709551615\r\n",
      "*1\r\n$3\r\nGETXX",
  };
  for (const std::string &input : invalid) {
    REQUIRE_EQ(parse_command(bytes(input)).status, ParseStatus::Invalid);
  }
}

TEST(parser_distinguishes_incomplete_bulk_components) {
  const std::vector<std::string> incomplete = {
      "*2\r\n",
      "*1\r\n$3",
      "*1\r\n$3\r\n",
      "*1\r\n$3\r\nGE",
      "*1\r\n$3\r\nGET\r",
  };
  for (const std::string &input : incomplete) {
    REQUIRE_EQ(parse_command(bytes(input)).status, ParseStatus::Incomplete);
  }
}

TEST(parser_supports_empty_binary_and_trailing_data) {
  std::string command = "*2\r\n$4\r\nECHO\r\n$3\r\n";
  command.push_back('\0');
  command.push_back('\r');
  command.push_back('\n');
  command += "\r\ntrailing";

  const ParseResult binary = parse_command(bytes(command));
  REQUIRE_EQ(binary.status, ParseStatus::Complete);
  REQUIRE_EQ(binary.args.size(), 2U);
  REQUIRE_EQ(binary.args[1], std::string("\0\r\n", 3));
  REQUIRE_EQ(binary.bytes_consumed, command.size() - 8U);

  const ParseResult empty = parse_command(bytes("*1\r\n$0\r\n\r\n"));
  REQUIRE_EQ(empty.status, ParseStatus::Complete);
  REQUIRE_EQ(empty.args, std::vector<std::string>({""}));
}
