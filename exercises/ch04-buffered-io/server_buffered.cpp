#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <assert.h>
#include "server.hpp"


static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg); //is the last argument to fprintf a pointer?
    // fprintf wants the pointer. It uses that pointer to walk through memory one character at a time until it reaches '\0'.
}


static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}


int read_asmuch(Conn* conn) {
    size_t capacity = sizeof(conn->rbuf);
    size_t space_left = capacity - conn->rbuf_size;
    if (space_left == 0) {
        return 0;
    }
    ssize_t rv = read(
        conn->fd,
        conn->rbuf + conn->rbuf_size, // pointer arithmatic to shift to next available empty slot in array
        space_left // read at most space_left bytes otherwise buffer_overflow error
    );

    if (rv < 0) {
        return -1;  // failed read() call
    }

    if (rv == 0) {
        if (conn->rbuf_size > 0) {
            msg("unexpected EOF");
        } else {
            msg("EOF");
        }
        return -1;
    }
    assert((size_t)rv <= space_left);
    conn->rbuf_size += (size_t)rv;
    return 0;
}


int write_asmuch(Conn* conn) {
    size_t remaining = conn->wbuf_size - conn->wbuf_sent;

    if (remaining == 0) {
        conn->wbuf_size = 0;
        conn->wbuf_sent = 0;
        conn->state = STATE_REQ;
        return 0;
    }

    ssize_t rv = write(
        conn->fd,
        conn->wbuf + conn->wbuf_sent,
        remaining
    );

    if (rv < 0) {
        return -1;
    }

    if (rv == 0) {
        return -1;
    }

    assert((size_t)rv <= remaining);
    conn->wbuf_sent += (size_t)rv;

    if (conn->wbuf_sent == conn->wbuf_size) {
        conn->wbuf_size = 0;
        conn->wbuf_sent = 0;
        conn->state = STATE_REQ;
    }

    return 0;
}


int try_one_request(Conn* conn){
    //detect full request
    uint32_t len = 0;

    if (conn->rbuf_size < 4){
        msg("not enough bytes");
        return 0; // not an error just means we need to keep reading
    }

    memcpy(&len, conn->rbuf, 4);

    if (len > k_max_msg) { // check if the message is too long
        msg("Too long");
        conn->state = STATE_END;
        return -1;
    }

    if (conn-> rbuf_size < 4 + len){ // valid length header, not a complete message
        return 0;
    }


    // process the request
    // or create a temp buffer, copy len bytes, add a null terminator, print the temp buffer
    printf("client says: %.*s\n", (int)len, conn->rbuf + 4);
    const char reply[] = "world";
    uint32_t wlen = (uint32_t)strlen(reply);    // update the wbuf_size

    memcpy(conn->wbuf, &wlen, 4);
    memcpy(conn->wbuf + 4, reply, wlen);

    conn-> wbuf_sent = 0;
    conn->wbuf_size = 4+ wlen;
    conn->state = STATE_RES;


    size_t consumed = 4 + len;
    size_t remaining = conn->rbuf_size - consumed;

    if (remaining > 0) {
        memmove(conn->rbuf, conn->rbuf + consumed, remaining);
    }
    conn->rbuf_size = remaining;
    return 1;
}

int connection_io(Conn* conn) {
    switch (conn->state) {
        case STATE_REQ: // if it's in a requesting state call read_asmuch
            if (read_asmuch(conn) < 0) { // if read_asmuch fails then end
                conn->state = STATE_END;
                return -1;
            }

            if (try_one_request(conn) < 0) { // call try_one_request if read_asmuch passes
                conn->state = STATE_END;
                return -1;
            }

            return 0;

        case STATE_RES:
            if (write_asmuch(conn) < 0) {
                conn->state = STATE_END;
                return -1;
            }

            return 0;

        case STATE_END:
            return -1;
    }

    conn->state = STATE_END;
    return -1;
}

int main(){

    int fd = socket(AF_INET, SOCK_STREAM, 0);


    if (fd < 0) {
            die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));


    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);




    addr.sin_addr.s_addr = htonl(0);

    int rv = bind(fd, (const struct sockaddr *) &addr, sizeof(addr));
    if ( rv < 0){
        die("bind()");
    }

    int rv1 = listen(fd, SOMAXCONN);

    if(rv1 < 0){
        die("listen()");
    }

    while (true) {
    // accept
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0) {
            continue; // error
        }
        Conn connection = {};
        connection.fd = connfd;
        connection.state = STATE_REQ;
        connection.rbuf_size = 0;
        connection.wbuf_size = 0;
        connection.wbuf_sent = 0;
        // only serves one client connection at once
        while (true) {
            int32_t err = connection_io(&connection);
            if (err) {
                break;
            }
        }
    close(connfd);
    }
}
