#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2,
};

static constexpr size_t k_max_msg = 4096;

struct Conn {
    int fd = -1;
    int state = STATE_REQ;

    size_t rbuf_size = 0;
    uint8_t rbuf[4 + k_max_msg];

    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];
};

int read_asmuch(Conn* conn);
// fills conn->rbuf from conn->fd

int write_asmuch(Conn* conn);
 //writes as many bytes as possible into the conn.wbuf()

int connection_io(Conn* conn);// ?
//tries to parse/process one complete request from conn->rbuf
int try_one_request(Conn* conn);
// looks at conn->state and decides which of the above to call




// Function declarations you want to expose later go below this.
// Example shape only:
// return_type function_name(parameter_types);
