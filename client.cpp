#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include "utils.h"


static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg); //is the last argument to fprintf a pointer?
    // fprintf wants the pointer. It uses that pointer to walk through memory one character at a time until it reaches '\0'.
}


static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

const size_t k_max_msg = 4096;

static int32_t query(int fd, const char *text) {

    // this function writes text and then has a buffer to read the response from the serer?
    uint32_t len = (uint32_t)strlen(text);

    if (len > k_max_msg) {
    return -1;
    }

    char wbuf[4+k_max_msg];
    memcpy(wbuf, &len, 4); // assume little endian
    memcpy(&wbuf[4], text, len);

    if( int32_t err = write_all(fd,wbuf,4+len)){
        return err;
    }

    char rbuf[4+k_max_msg+1];

    errno=0;

    int32_t err = read_full(fd,rbuf,4);

    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    memcpy(&len, rbuf, 4); // assume little endian

    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);

    if (err) {
        msg("read() error");
        return err;
    }
    rbuf[4 + len] ='\0';
    printf("server says: %s\n", &rbuf[4]); // print what the client says
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);  // 127.0.0.1
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }

    int32_t err = query(fd, "hello1");
    if (err) {
        goto L_DONE;
    }

    err = query(fd, "hello2");
    if (err) {
        goto L_DONE;
    }

    err = query(fd, "hello3");

    if (err) {
        goto L_DONE;
    }

    L_DONE:
        close(fd);
        return 0;
}
