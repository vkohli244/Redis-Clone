#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include "utils.h"
#include <assert.h>



int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
      std::cerr << "setsockopt failed\n";
      return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);

  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  std::vector<struct pollfd> fds;
  std::vector<Conn *> fd2conn;

  int numClients = 0;

  const char *response = "+PONG\r\n";
  while (true) {
      fds.clear(); // clear the fds list on every iteration

      struct pollfd listener = {server_fd, POLLIN, 0}; // rebuild the listening socket pollfd struct on each iteration
      fds.push_back(listener);

      for (Conn *conn : fd2conn){
          if(!conn){ // if there is no conn struct in this index
              continue;
          }
          struct pollfd pfd = {conn->fd, 0, 0}; // currently POLLIN since we have no bool flags

          if (conn->want_read){
              pfd.events |= POLLIN;
          }

          fds.push_back(pfd);
      }

      int rv = poll(fds.data(),(nfds_t) fds.size(), -1);

      if (rv < 0 && errno == EINTR){
          continue; // No error occured build the polled fds vector again
      }

      if (rv < 0){
          die("poll");
      }

      // Check the listening socket (index 0) for a new connection
      if (fds[0].revents & POLLIN) {
          if(Conn *conn = handle_accept(server_fd)){
              std::cout << "Client Connected";
              if(fd2conn.size() <= (size_t)conn->fd){
                  fd2conn.resize(conn->fd + 1);
              }

              fd2conn[conn->fd] = conn;
          }
      }

      // Check every client (index 1 onward) for incoming data
      for (size_t i = 1; i < fds.size(); ++i) { // note: skip the 1st
          uint32_t ready = fds[i].revents;

          if (ready == 0) {
              continue;
          }

          Conn *conn = fd2conn[fds[i].fd];

          if(ready & POLLIN){
              assert(conn->want_read);
              handle_read(conn);
          }

          if(ready & POLLERR ||conn->want_close){
              std::cout << "Client closed\n";
              fd2conn[fds[i].fd] = NULL;
              close(fds[i].fd);
              delete conn;
          }
      }
    }
    close(server_fd);
    return 0;
  }
