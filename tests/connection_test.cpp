#include "command_handler.h"
#include "connection.h"
#include "database.h"
#include "test_support.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <vector>

namespace {
struct SocketPair {
  SocketPair() {
    int descriptors[2] = {-1, -1};
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0);
    server.reset(descriptors[0]);
    client.reset(descriptors[1]);
    set_nonblocking(server.get());
  }

  FileDescriptor server;
  FileDescriptor client;
};

} // namespace

TEST(connection_reads_fragmented_requests_and_preserves_state) {
  SocketPair sockets;
  Database database;
  CommandHandler handler(database);
  Connection connection(sockets.server.get());

  const std::string request = resp({"ECHO", "fragmented"});
  send_all(sockets.client.get(), std::string_view(request).substr(0, 9));
  connection.handle_read(handler);
  REQUIRE(connection.want_read);
  REQUIRE(!connection.want_write);
  REQUIRE(!connection.incoming.empty());

  send_all(sockets.client.get(), std::string_view(request).substr(9));
  connection.handle_read(handler);
  REQUIRE(!connection.want_read);
  REQUIRE(connection.want_write);
  REQUIRE(connection.incoming.empty());

  connection.handle_write();
  REQUIRE_EQ(receive_exactly(sockets.client.get(), 17),
             "$10\r\nfragmented\r\n");
  REQUIRE(connection.want_read);
  REQUIRE(!connection.want_write);
}

TEST(connection_drains_every_complete_pipelined_command) {
  SocketPair sockets;
  Database database;
  CommandHandler handler(database);
  Connection connection(sockets.server.get());

  const std::string requests = resp({"SET", "key", "value"}) +
                               resp({"GET", "key"}) + resp({"PING"});
  const std::string expected = "+OK\r\n$5\r\nvalue\r\n+PONG\r\n";
  send_all(sockets.client.get(), requests);
  connection.handle_read(handler);
  REQUIRE_EQ(connection.outgoing, bytes(expected));
  REQUIRE(connection.incoming.empty());

  connection.handle_write();
  REQUIRE_EQ(receive_exactly(sockets.client.get(), expected.size()), expected);
}

TEST(connection_rejects_invalid_resp_and_closes_on_empty_eof) {
  Database database;
  CommandHandler handler(database);

  {
    SocketPair sockets;
    Connection connection(sockets.server.get());
    send_all(sockets.client.get(), "not-resp\r\n");
    connection.handle_read(handler);
    REQUIRE(connection.want_close);
  }

  {
    SocketPair sockets;
    Connection connection(sockets.server.get());
    sockets.client.reset();
    connection.handle_read(handler);
    REQUIRE(connection.want_close);
  }
}

TEST(connection_flushes_a_response_after_peer_half_close) {
  SocketPair sockets;
  Database database;
  CommandHandler handler(database);
  Connection connection(sockets.server.get());

  send_all(sockets.client.get(), resp({"PING"}));
  REQUIRE(shutdown(sockets.client.get(), SHUT_WR) == 0);
  connection.handle_read(handler);
  REQUIRE(connection.want_write);
  REQUIRE(!connection.want_close);

  connection.handle_write();
  REQUIRE_EQ(receive_exactly(sockets.client.get(), 7), "+PONG\r\n");
  REQUIRE(connection.want_close);
  REQUIRE(!connection.want_read);
}

TEST(connection_handles_partial_writes_without_losing_bytes) {
  SocketPair sockets;
  int send_buffer_size = 4096;
  REQUIRE(setsockopt(sockets.server.get(), SOL_SOCKET, SO_SNDBUF,
                     &send_buffer_size, sizeof(send_buffer_size)) == 0);
  set_nonblocking(sockets.client.get());

  Connection connection(sockets.server.get());
  constexpr std::size_t payload_size = 2U * 1024U * 1024U;
  connection.outgoing.assign(payload_size, static_cast<std::uint8_t>('x'));
  connection.want_read = false;
  connection.want_write = true;

  connection.handle_write();
  REQUIRE(connection.want_write);
  REQUIRE(!connection.outgoing.empty());

  std::size_t received = 0;
  std::vector<char> buffer(64U * 1024U);
  while (connection.want_write) {
    while (true) {
      const ssize_t count =
          recv(sockets.client.get(), buffer.data(), buffer.size(), 0);
      if (count > 0) {
        received += static_cast<std::size_t>(count);
        continue;
      }
      REQUIRE(count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
      break;
    }
    connection.handle_write();
  }

  while (true) {
    const ssize_t count = recv(sockets.client.get(), buffer.data(), buffer.size(), 0);
    if (count > 0) {
      received += static_cast<std::size_t>(count);
      continue;
    }
    REQUIRE(count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
    break;
  }
  REQUIRE_EQ(received, payload_size);
}

TEST(connection_closes_when_the_peer_disconnects_before_a_write) {
  SocketPair sockets;
  Connection connection(sockets.server.get());
  connection.outgoing = bytes("pending");
  connection.want_read = false;
  connection.want_write = true;
  sockets.client.reset();

  connection.handle_write();
  REQUIRE(connection.want_close);
}

TEST(connection_handle_request_leaves_incomplete_data_buffered) {
  Database database;
  CommandHandler handler(database);
  Connection connection(-1);
  connection.incoming = bytes("*1\r\n$4\r\nPIN");
  connection.handle_request(handler);
  REQUIRE(connection.want_read);
  REQUIRE(!connection.want_write);
  REQUIRE_EQ(connection.incoming, bytes("*1\r\n$4\r\nPIN"));
}
