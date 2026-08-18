#pragma once

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

struct TestCase {
  const char *name;
  void (*function)();
};

inline std::vector<TestCase> &test_registry() {
  static std::vector<TestCase> tests;
  return tests;
}

class TestRegistrar {
public:
  TestRegistrar(const char *name, void (*function)()) {
    test_registry().push_back({name, function});
  }
};

#define TEST(name)                                                             \
  static void name();                                                          \
  static TestRegistrar name##_registrar(#name, &name);                         \
  static void name()

#define REQUIRE(condition)                                                     \
  do {                                                                         \
    if (!(condition)) {                                                        \
      throw std::runtime_error(std::string("requirement failed: ") +          \
                               #condition + " at " + __FILE__ + ":" +         \
                               std::to_string(__LINE__));                       \
    }                                                                          \
  } while (false)

#define REQUIRE_EQ(actual, expected)                                           \
  do {                                                                         \
    const auto &actual_value = (actual);                                        \
    const auto &expected_value = (expected);                                    \
    if (!(actual_value == expected_value)) {                                    \
      throw std::runtime_error(std::string("equality failed: ") + #actual +    \
                               " == " + #expected + " at " + __FILE__ + ":" + \
                               std::to_string(__LINE__));                       \
    }                                                                          \
  } while (false)

inline std::vector<std::uint8_t> bytes(std::string_view value) {
  return {value.begin(), value.end()};
}

inline std::string resp(const std::vector<std::string> &args) {
  std::string result = "*" + std::to_string(args.size()) + "\r\n";
  for (const std::string &arg : args) {
    result += "$" + std::to_string(arg.size()) + "\r\n" + arg + "\r\n";
  }
  return result;
}

class FileDescriptor {
public:
  FileDescriptor() = default;
  explicit FileDescriptor(int fd) : fd_(fd) {}
  ~FileDescriptor() { reset(); }

  FileDescriptor(const FileDescriptor &) = delete;
  FileDescriptor &operator=(const FileDescriptor &) = delete;

  FileDescriptor(FileDescriptor &&other) noexcept
      : fd_(std::exchange(other.fd_, -1)) {}

  FileDescriptor &operator=(FileDescriptor &&other) noexcept {
    if (this != &other) {
      reset(std::exchange(other.fd_, -1));
    }
    return *this;
  }

  int get() const { return fd_; }

  void reset(int fd = -1) {
    if (fd_ >= 0) {
      close(fd_);
    }
    fd_ = fd;
  }

private:
  int fd_ = -1;
};

inline void set_nonblocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  REQUIRE(flags >= 0);
  REQUIRE(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

inline void send_all(int fd, std::string_view data) {
  std::size_t sent = 0;
  while (sent < data.size()) {
#ifdef MSG_NOSIGNAL
    const ssize_t count =
        send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
#else
    const ssize_t count = send(fd, data.data() + sent, data.size() - sent, 0);
#endif
    if (count > 0) {
      sent += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      struct pollfd pfd = {fd, POLLOUT, 0};
      REQUIRE(poll(&pfd, 1, 2000) > 0);
      continue;
    }
    throw std::runtime_error(std::string("send failed: ") +
                             std::strerror(errno));
  }
}

inline std::string receive_exactly(int fd, std::size_t expected_size,
                                   int timeout_ms = 5000) {
  std::string result;
  result.resize(expected_size);
  std::size_t received = 0;
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms);

  while (received < expected_size) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    REQUIRE(remaining.count() > 0);

    struct pollfd pfd = {fd, POLLIN, 0};
    const int ready = poll(&pfd, 1, static_cast<int>(remaining.count()));
    REQUIRE(ready > 0);

    const ssize_t count =
        recv(fd, result.data() + received, result.size() - received, 0);
    if (count > 0) {
      received += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    throw std::runtime_error("connection closed before expected response");
  }
  return result;
}
