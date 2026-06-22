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

static int32_t one_request(int connfd) {
// 4 bytes header
    char rbuf[4 + k_max_msg + 1]; // char arrays are always array-to-pointer decay in memory
    errno = 0;
    int32_t err = read_full(connfd,rbuf,4); // read the length, we keep it signed because it may return an error
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0; // unsigned because length should never be negative

    memcpy(&len, rbuf, 4); // assume little endian, copy the first 4 bytes of the buffer into len as an uint32
    //get the length

    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }


    err = read_full(connfd,&rbuf[4],len);

    if (err){
        msg("read() error");
        return -1;
    }

    rbuf[4 + len] ='\0'; // terminate the string
    printf("client says: %s\n", &rbuf[4]); // print what the client says

    const char response[] = "world";

    char wbuf[4 + sizeof(response)];

    len = (uint32_t)strlen(response); // set len now the length of the string

    memcpy(wbuf, &len,4); // put the length of the string into the first 4 bytes of the buffer as an again an unsigned int
    // since we're using the first 4 bytes for the length of the string since 4 bytes = 32 bits we have to use uint32_t instead of using size_t
    memcpy(&wbuf[4],response,len); // in the previous line we didn't need the & because of the array to pointer decay, wbuf -> &wbuf[0]
    // memcpy expects a pointer, just doing memcpy[4] would give the actual value at the location of an offset of 4 and treat that as a memory address

 return write_all(connfd, wbuf, 4 + len);
}

void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        msg("read() error");
        return;
    }
    fprintf(stderr, "client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}


int main(){

    int fd = socket(AF_INET, SOCK_STREAM, 0); // create a socket for the server for TCP protocol on IPv4 addresses

    // if fd < 0 that means error creating a fd resource from kernel
    if (fd < 0) {
            die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    //configure the socket IPv4 address, has address family, port number, and IP address
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234); //configuring the port to 1234 so this would mean if we have an IPv4 address after it ends the port is after ":"

    // ntohs() is network to host short, this is convertin network byte order which is big endian (most sig byte first) to the host architecture's endianess
    //
    //
    addr.sin_addr.s_addr = htonl(0); // use htonl() or host to network long since IPv4 address are 32 bit (4 byte) ints converting host byte order ( little or big endian)
    // to network byte order which is big endian, even though 0 doesn't really make a difference it's for semantic and clarity reasons
    // sin_addr is type struct in_addr which contains only
    // struct in_addr {
    //        unsigned long s_addr;
    //};
    // This struct is different on many systems so to make the API more portable across systems it's wrapped in a struct

    int rv = bind(fd, (const struct sockaddr *) &addr, sizeof(addr));
    if ( rv < 0){
        die("bind()");
    }

    int listenfd = listen(fd, SOMAXCONN); // listen(int, int), first arg is fd, second arg is max conns use SOMAXCONN ( macro specified by network std lib?)

    if(listenfd < 0){
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
        // only serves one client connection at once
        while (true) {
            int32_t err = one_request(connfd);
            if (err) {
                break;
            }
        }
    close(connfd);
    }
}
