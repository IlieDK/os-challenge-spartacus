#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include "messages.h"
#include "lonesha256.h"


#define LONESHA256_STATIC
#define BACKLOG 20

int recvall(int socket, uint8_t *buf, size_t *len); 
int sendall(int socket, const uint8_t *buf, size_t *len);
int lonesha256(unsigned char out[32], const unsigned char *in, size_t len);

int main(int argc, char *argv[]) {

    uint8_t buffer[PACKET_REQUEST_SIZE];

    // command line part
    if (argc != 2) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        return 1;
    }

    char *end;
    long port = strtol(argv[1], &end, 10);   // function that converts a string to a long integer, interpreting the string according to the specified base
    if (*end != '\0' || port < 1 || port > 65535) {
        fprintf(stderr, "invalid port: %s\n", argv[1]);
        return 1;
    }

    // socket part
    int socket_descriptor;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);


    socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_descriptor == -1) {
        perror("socket");
        return 1;
    }

    int yes = 1;
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        return 1;
    }

    // bind part
    if (bind(socket_descriptor, (struct sockaddr *)&addr, sizeof addr) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(socket_descriptor, BACKLOG) == -1) {
        perror("listen");
        return 1;
    }

    while (1){

        int new_request = accept(socket_descriptor, NULL, NULL);

        if (new_request == -1){
            perror("accept");
            continue;
        }

        size_t recv_len = sizeof(buffer);

        if (recvall(new_request, buffer, &recv_len) == -1) {
            fprintf(stderr, "incomplete request: got %zu bytes\n", recv_len);
            close(new_request);
            continue;
        } 
        
        // calculations
        uint64_t answer = htobe64(1);

        size_t send_len = sizeof(answer);
        if (sendall(new_request, (const uint8_t *)&answer, &send_len) == -1) {
            fprintf(stderr, "incomplete send: sent %zu bytes\n", send_len);
            close(new_request);
            continue;
        }

        close(new_request);
    }

    
    close(socket_descriptor);
    
    return 0;
}


int recvall(int socket, uint8_t *buf, size_t *len) {


    size_t total = 0;
    size_t bytesleft = *len;
    ssize_t n = 0;

    while (total < *len) {
        n = recv(socket, buf + total, bytesleft, 0);
        if (n <= 0)              // 0: client closed early, -1: error
            break;
        total += (size_t)n;
        bytesleft -= (size_t)n;
    }

    *len = total;

    return n <= 0 ? -1 : 0;
}


int sendall(int socket, const uint8_t *buf, size_t *len) {

    size_t total = 0; // how many bytes we've sent
    size_t bytesleft = *len; // how many we have left to send
    ssize_t n = 0;

    while(total < *len) {

        n = send(socket, buf+total, bytesleft, MSG_NOSIGNAL);
        if (n == -1) { 
            break; 
        }

        total += (size_t)n;
        bytesleft -= (size_t)n;
    }

    *len = total; // return number actually sent here

    return n == -1 ? -1 : 0;
}