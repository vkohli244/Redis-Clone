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
