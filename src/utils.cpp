#pragma once
#include "utils.h"
#include "parser.h"
#include <string>
#include <cstring>
#include <sys/socket.h>

void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    int rv = fcntl(fd, F_SETFL, flags);
    if (rv < 0) {
        die("fcntl error");
    }
}


Conn *handle_accept(int fd){

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_fd  < 0){
        msg_errno("accept() error");
        return NULL;
    }

    fd_set_nb(client_fd);

    Conn *conn = new Conn;
    conn->fd = client_fd;
    conn->want_read = true;
    return conn;
}

void bufappend(std::vector<uint8_t> &buf, const char *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}


void handle_read(Conn *conn){
    char buffer[64 * 1024];

    while(true){
        ssize_t bytes_recieved = recv(conn->fd, buffer, sizeof(buffer), 0);

        if (bytes_recieved > 0){
            bufappend(conn->incoming, buffer, bytes_recieved);
            continue;
        }

        if(bytes_recieved == 0){
            // end of file
            conn->want_close = true;
            msg("eof");
            return;
        }

        if (bytes_recieved < 0 && errno == EINTR){
            continue;
        }

        if( bytes_recieved < 0 && (errno == EAGAIN||errno == EWOULDBLOCK)){ // if there were no bytes recieved and blocking or timeout ran out
            break; // why do we break out of the loop?
        }

        if( bytes_recieved < 0){
            msg_errno("recv error");
            conn->want_close = true;
            return;
        }

    } // end of while




}


void handle_request(Conn *conn){

    ParseResult result = small_parse(conn->incoming);

    if(result.status == ParseStatus::Incomplete){
        return;
    }

    if(result.status == ParseStatus::Invalid){
        msg("Invalud RESP request");
        conn->want_close = true;
    }

    if(result.args.empty()){
        msg("No arguments present");
        conn->want_close = true;
        return;
    }

    // Next step is to handle writes
    //
    const std::string &command = result.args[0];



    if (command == "PING"){
        const char* response = "+PONG";
        send(conn->fd, response, std::strlen(response),0);
    }
    else if(command == "ECHO"){
        if(result.args.size() != 2){
            const char* response = "-ERR wrong number of arguments for 'ECHO' command\r\n";
            send(conn->fd, response, std::strlen(response),0);
        }
        else{
            const std::string &message = result.args[1];
            const std::string response = "$"
                                        + std::to_string(message.size())
                                        + "\r\n"
                                        + message
                                        + "\r\n";
            send(conn->fd,response.data(), response.size(),0);
        }
    }
    else{
        const char* response = "-ERR unknown command\r\n";
        send(conn->fd, response, std::strlen(response),0);
    }

    // now write bufconsume

}
