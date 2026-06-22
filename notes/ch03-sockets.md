# Chapter 3 - Hello Server/Client

## What This Chapter Builds

This chapter builds a basic TCP server and client.

The server:

- Creates a TCP socket
- Binds to a local port
- Listens for incoming connections
- Accepts one client connection
- Reads a message from the client
- Sends a response back

The client:

- Creates a TCP socket
- Connects to the server
- Sends a message
- Reads the server response

## Files Changed

- `server.cpp`
- `client.cpp`

## Key Concepts

### File Descriptors

A socket is represented by a file descriptor. The file descriptor is an integer handle that refers to a kernel-managed socket object.

### Server Socket Flow

```text
socket -> setsockopt -> bind -> listen -> accept
```

### Client Socket Flow

```text
socket -> connect -> write -> read
```

### Listening Socket vs Connected Socket

The listening socket accepts new connections.

The connected socket returned by `accept()` is used to communicate with one specific client.

### TCP Is Two-Way

Both the client and server can call `read()` and `write()` on their socket file descriptors.

The connection is full-duplex:

```text
client write -> server read
server write -> client read
```

## What I Implemented

- Basic server setup using IPv4 and TCP
- `SO_REUSEADDR` to make restarting the server easier
- Binding to port `1234`
- Listening for connections
- Accepting one client at a time
- Simple request/response: client sends `"hello"`, server replies `"world"`

## What Confused Me

- Why `accept()` returns a new file descriptor
- Why `struct sockaddr_in` needs to be cast to `struct sockaddr *`
- Why ports use byte-order conversion
- Why the server binds but the client connects

## What I Learned

The server socket is not the same as the connected client socket. The original socket listens for new clients. The socket returned from `accept()` is for communication with one client.
