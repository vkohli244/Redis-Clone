# Redis Clone in C/C++

This repository is my learning implementation while following *Build Your Own Redis with C/C++* by James Smith. The goal is not just to copy the final code, but to understand how Redis-like servers are built from lower-level systems concepts: TCP sockets, file descriptors, protocol parsing, buffering, event loops, and in-memory data structures.

## Current Status

2026.06.22 - I am currently working through Chapter 4: Protocol Parsing.

Implemented so far:

- Basic TCP server and client using POSIX sockets
- Server socket setup with `socket`, `setsockopt`, `bind`, `listen`, and `accept`
- Client connection using `connect`
- Full-duplex client/server communication over TCP
- Length-prefixed request/response protocol
- `read_full` and `write_all` helpers for handling partial reads and writes
- Shared `utils.h` / `utils.cpp` for common socket I/O helpers

## Project Goals

The main goal is to understand how a Redis-like server works internally.

Specific learning goals:

- Understand how TCP sockets expose network communication as file descriptors
- Understand why TCP is a byte stream and not a message protocol
- Build a simple application protocol using length-prefixed messages
- Learn how to safely parse requests from a stream of bytes
- Learn why production servers use buffering and event loops
- Eventually implement Redis-like commands such as `GET`, `SET`, and `DEL`


## How to Run

In one terminal:

```bash
make run-server
```

In another terminal:

```bash
make run-client
```

## Protocol

The current protocol uses length-prefixed messages:

```text
[4-byte length][message body]
```

For example, the string `"hello"` is sent as:

```text
[5][h e l l o]
```

The 4-byte length tells the receiver how many bytes to read for the body.

This is necessary because TCP does not preserve application-level message boundaries. A single `write()` from the client may be split across multiple `read()` calls on the server, and multiple client writes may also be combined into one server read.

## Notes

I am keeping chapter notes in the `notes/` folder.

Current notes:

- `notes/ch03-sockets.md`
- `notes/ch04-protocol-parsing.md`

These notes explain what I implemented, what confused me, and what I learned in my own words.

## Exercises and Experiments

I am also keeping experiments in the `experiments/` folder and exercises in the `exercises/` folder

Planned/current Exercises:
- Ch4: Buffered I/O design to reduce syscall count 


## Learning Focus

This project is mainly about systems programming fundamentals:

- File descriptors
- Kernel/user-space boundaries
- TCP sockets
- Byte order
- Partial reads and writes
- Message framing
- Buffer management
- Event-driven server architecture

## Disclaimer

This is a learning project based on *Build Your Own Redis with C/C++*. The base idea follows the book, but the notes, explanations, refactors, experiments, and additional design work are my own.
