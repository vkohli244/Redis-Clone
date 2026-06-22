# Learning Log

This file tracks what I learned while building this Redis clone. I am using it to document my understanding, not just the final code.

## Current Chapter: Chapter 4 - Protocol Parsing

### What I Implemented

- A length-prefixed request protocol
- `read_full` to repeatedly call `read()` until exactly `n` bytes are read
- `write_all` to repeatedly call `write()` until exactly `n` bytes are written
- `one_request` on the server to read one complete request and send one response
- `query` on the client to send one complete request and read one response
- Shared utility files for common I/O helpers

### Main Realization

TCP is not a message protocol. It is a byte stream.

That means if the client sends:

```text
[request 1][request 2]
```

the server might receive:

```text
part of request 1
```

or:

```text
all of request 1 and part of request 2
```

or:

```text
both requests together
```

Because of that, the application needs its own protocol for knowing where one message ends and the next begins.

The current protocol solves this by sending:

```text
[4-byte length][message body]
```

The receiver first reads 4 bytes to know the body length, then reads exactly that many bytes for the body.

### Things That Confused Me

#### `read(fd, buf, n)`

I initially thought `read()` would read exactly `n` bytes. It does not.

It reads **up to** `n` bytes. It may return fewer bytes. That is why `read_full()` is needed.

#### `ssize_t` vs `size_t`

`size_t` is unsigned and represents sizes.

`ssize_t` is signed because functions like `read()` and `write()` need to return `-1` on error.

#### `memcpy(&wbuf[4], reply, len)`

`memcpy()` needs an address, so the destination must be `&wbuf[4]` or equivalently `wbuf + 4`.

#### Array-to-pointer decay

When passing an array like `wbuf` to a function, it usually decays to a pointer to the first element:

```c
wbuf
```

acts like:

```c
&wbuf[0]
```

That is why this works:

```c
memcpy(wbuf, &len, 4);
```

It copies into the beginning of the buffer.

### What I Understand Now

- A socket file descriptor can be read from and written to
- TCP connections are full-duplex
- The client and server each have their own file descriptor for the same connection
- `read()` and `write()` may complete partially
- Protocol parsing is about turning a raw byte stream into meaningful messages
- The length prefix is a simple form of message framing
- `memcpy()` is used to copy raw bytes into and out of buffers

### Current Questions

2026.06.12:
- How should buffered I/O be structured?
- How do real servers avoid one syscall per protocol component?
- How does Redis handle many clients at once?
- How will the event loop change the current blocking design?

### Next Steps

- Add notes for Chapter 4
- Implement the buffered I/O exercise
- Test multiple requests on the same TCP connection
- Start learning how the event loop will replace the blocking loop
