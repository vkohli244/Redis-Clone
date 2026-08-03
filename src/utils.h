#include <iostream>
#include <fcntl.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>


struct Conn{
    int fd = -1;

    bool want_read = false;
    bool want_close = false;
    bool want_write = false;
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};


Conn * handle_accept(int fd);

void fd_set_nb(int fd);

void msg(const char *msg0);

void msg_errno(const char *msg);

void die(const char *msg);

void handle_read(Conn *conn);

void bufappend(std::vector<uint8_t> &buf, const char *data, size_t len);

void handle_request(Conn *conn);

bool handle_write(Conn *conn);

void bufconsume(std::vector<uint8_t> &buf, std::size_t count);
