# Chapter 4 - Protocol Parsing

## What This Chapter Builds

This chapter adds a simple application protocol on top of TCP.

Instead of reading arbitrary bytes, the client and server now exchange length-prefixed messages:

```text
[4-byte length][message body]
```

This allows the receiver to know exactly how many bytes belong to one message.

## Files Changed

- `server.cpp`
- `client.cpp`
- `utils.h`
- `utils.cpp`

## Key Concepts

### TCP Is a Byte Stream

TCP does not preserve message boundaries. If the client sends one message, the server may receive it in pieces. If the client sends multiple messages, the server may receive them together.

### Message Framing

Message framing means defining where each application-level message starts and ends.

This project currently uses a 4-byte length prefix.

### Partial Reads

`read(fd, buf, n)` reads up to `n` bytes. It may read fewer.

That is why `read_full()` loops until exactly the required number of bytes has been received.

### Partial Writes

`write(fd, buf, n)` may write fewer than `n` bytes.

That is why `write_all()` loops until all bytes have been sent.

## What I Implemented

- `read_full`
- `write_all`
- `one_request`
- Client `query`
- Length-prefixed request and response messages

## Important Code Paths

Server request flow:

```text
read 4-byte length
validate length
read request body
process request
write 4-byte response length
write response body
```

Client query flow:

```text
write 4-byte request length
write request body
read 4-byte response length
read response body
print response
```

## What Confused Me

- Why `read()` does not necessarily return the full requested amount
- Why `ssize_t` is used for `read()` and `write()` return values
- Why `memcpy()` is used to encode and decode the 4-byte length
- Why `&wbuf[4]` is needed when copying into the buffer after the header

## What I Learned

The protocol is separate from TCP. TCP only moves bytes. The protocol gives those bytes structure.

The 4-byte length prefix lets the receiver know how many bytes to collect before it has one complete request.

## Deviations / Experiments

- Moved `read_full` and `write_all` into shared utility files
- Added notes explaining pointer arithmetic and array-to-pointer decay
- Planning a buffered I/O version to reduce syscall count

## Questions for Later

- How can buffered I/O reduce the number of syscalls?
- How do we handle multiple requests already sitting in the read buffer?
- How does the server avoid blocking on one client?
- How will an event loop change the design?
