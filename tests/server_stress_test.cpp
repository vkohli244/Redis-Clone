#include "server.h"
#include "test_support.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace {
class RunningServer {
public:
  RunningServer() : thread_([this] { result_.store(server_.run()); }) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (server_.port() == 0 && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(1ms);
    }
    if (server_.port() == 0) {
      server_.request_stop();
      thread_.join();
      throw std::runtime_error("server did not start");
    }
  }

  ~RunningServer() {
    server_.request_stop();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  std::uint16_t port() const { return server_.port(); }
  int result() const { return result_.load(); }

private:
  RedisServer server_{0};
  std::atomic<int> result_{-1};
  std::thread thread_;
};

FileDescriptor connect_client(std::uint16_t port) {
  FileDescriptor client(socket(AF_INET, SOCK_STREAM, 0));
  REQUIRE(client.get() >= 0);

  struct sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  REQUIRE(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
  REQUIRE(connect(client.get(), reinterpret_cast<struct sockaddr *>(&address),
                  sizeof(address)) == 0);
  return client;
}

template <typename Function>
void run_concurrently(std::size_t count, Function function) {
  std::atomic<bool> start{false};
  std::mutex errors_mutex;
  std::vector<std::exception_ptr> errors;
  std::vector<std::thread> threads;
  threads.reserve(count);

  for (std::size_t index = 0; index < count; ++index) {
    threads.emplace_back([&, index] {
      while (!start.load()) {
        std::this_thread::yield();
      }
      try {
        function(index);
      } catch (...) {
        std::lock_guard<std::mutex> lock(errors_mutex);
        errors.push_back(std::current_exception());
      }
    });
  }

  start.store(true);
  for (std::thread &thread : threads) {
    thread.join();
  }
  if (!errors.empty()) {
    std::rethrow_exception(errors.front());
  }
}
} // namespace

TEST(server_starts_on_an_ephemeral_port_and_stops_while_idle) {
  RunningServer server;
  REQUIRE(server.port() != 0);

  FileDescriptor client = connect_client(server.port());
  send_all(client.get(), resp({"PING"}));
  REQUIRE_EQ(receive_exactly(client.get(), 7), "+PONG\r\n");
  REQUIRE_EQ(server.result(), -1);
}

TEST(server_flushes_the_final_response_after_a_client_half_close) {
  RunningServer server;
  FileDescriptor client = connect_client(server.port());
  send_all(client.get(), resp({"ECHO", "last-response"}));
  REQUIRE(shutdown(client.get(), SHUT_WR) == 0);
  REQUIRE_EQ(receive_exactly(client.get(), 20), "$13\r\nlast-response\r\n");
}

TEST(server_rejects_a_second_listener_on_the_same_port) {
  RunningServer first;
  RedisServer second(first.port());
  REQUIRE_EQ(second.run(), 1);
}

TEST(server_multiplexes_256_simultaneous_fragmented_clients) {
  RunningServer server;
  constexpr std::size_t client_count = 256;
  std::vector<FileDescriptor> clients;
  clients.reserve(client_count);
  for (std::size_t index = 0; index < client_count; ++index) {
    clients.push_back(connect_client(server.port()));
  }

  run_concurrently(client_count, [&](std::size_t index) {
    const std::string key = "key:" + std::to_string(index);
    const std::string value = "value:" + std::to_string(index);
    const std::string request = resp({"SET", key, value}) +
                                resp({"GET", key}) + resp({"ECHO", key}) +
                                resp({"PING"});
    const std::string expected = "+OK\r\n$" + std::to_string(value.size()) +
                                 "\r\n" + value + "\r\n$" +
                                 std::to_string(key.size()) + "\r\n" + key +
                                 "\r\n+PONG\r\n";
    const std::size_t split = 1U + (index % (request.size() - 1U));
    send_all(clients[index].get(), std::string_view(request).substr(0, split));
    std::this_thread::yield();
    send_all(clients[index].get(), std::string_view(request).substr(split));
    REQUIRE_EQ(receive_exactly(clients[index].get(), expected.size()), expected);
  });
}

TEST(server_drains_deep_pipelines_without_additional_client_writes) {
  RunningServer server;
  constexpr std::size_t client_count = 32;
  constexpr std::size_t commands_per_client = 200;

  run_concurrently(client_count, [&](std::size_t) {
    FileDescriptor client = connect_client(server.port());
    std::string requests;
    std::string expected;
    for (std::size_t command = 0; command < commands_per_client; ++command) {
      requests += resp({"PING"});
      expected += "+PONG\r\n";
    }
    send_all(client.get(), requests);
    REQUIRE_EQ(receive_exactly(client.get(), expected.size()), expected);
  });
}

TEST(server_keeps_fast_clients_moving_while_many_requests_are_incomplete) {
  RunningServer server;
  constexpr std::size_t slow_client_count = 96;
  constexpr std::size_t fast_client_count = 128;
  std::vector<FileDescriptor> slow_clients;
  slow_clients.reserve(slow_client_count);

  const std::string incomplete = "*2\r\n$4\r\nECHO\r\n$100000\r\npartial";
  for (std::size_t index = 0; index < slow_client_count; ++index) {
    slow_clients.push_back(connect_client(server.port()));
    send_all(slow_clients.back().get(), incomplete);
  }

  run_concurrently(fast_client_count, [&](std::size_t index) {
    FileDescriptor client = connect_client(server.port());
    const std::string value = "fast:" + std::to_string(index);
    const std::string request = resp({"ECHO", value});
    const std::string expected = "$" + std::to_string(value.size()) + "\r\n" +
                                 value + "\r\n";
    send_all(client.get(), request);
    REQUIRE_EQ(receive_exactly(client.get(), expected.size()), expected);
  });
}

TEST(server_survives_connection_churn_and_abrupt_disconnects) {
  RunningServer server;
  constexpr std::size_t churn_count = 1000;
  for (std::size_t index = 0; index < churn_count; ++index) {
    FileDescriptor client = connect_client(server.port());
    if (index % 3U == 0) {
      send_all(client.get(), resp({"ECHO", std::string(4096, 'x')}));
    } else if (index % 3U == 1) {
      send_all(client.get(), "*2\r\n$4\r\nECHO\r\n$999\r\ntruncated");
    }
  }

  run_concurrently(64, [&](std::size_t) {
    FileDescriptor client = connect_client(server.port());
    send_all(client.get(), resp({"PING"}));
    REQUIRE_EQ(receive_exactly(client.get(), 7), "+PONG\r\n");
  });
}
