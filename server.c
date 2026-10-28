#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include "messages.h"
#include <stdbool.h>

#define LONESHA256_STATIC
#include "lonesha256.h"

#define BACKLOG 20

int recvall(int socket, uint8_t *buf, size_t *len); 
int sendall(int socket, const uint8_t *buf, size_t *len);
bool reverse_hash(const uint8_t *buf, uint64_t start, uint64_t end, uint64_t *answer);


int main(int argc, char *argv[]) {

    uint8_t buffer[PACKET_REQUEST_SIZE] = {0};

    // command line part
    if (argc != 2) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        return 1;
    }

    char *end_arg;
    long port = strtol(argv[1], &end_arg, 10);   // function that converts a string to a long integer, interpreting the string according to the specified base
    if (*end_arg != '\0' || port < 1 || port > 65535) {
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

    int temp_placeholder[100] = {0};

    size_t count = 0;
                                        
    while (count < sizeof temp_placeholder / sizeof temp_placeholder[0]) {

        int request = accept(socket_descriptor, NULL, NULL);

        if (request == -1) {
            perror("accept");
            continue;                                    // count unchanged: slot stays free
        }

        temp_placeholder[count++] = request;             // store, then advance
    }

    for (size_t index = 0; index < count; index++) {
    
        int new_request = temp_placeholder[index];

        size_t recv_len = sizeof(buffer);

        if (recvall(new_request, buffer, &recv_len) == -1) {
            fprintf(stderr, "incomplete request: got %zu bytes\n", recv_len);
            close(new_request);
            continue;
        } 

        uint64_t start, end;
        memcpy(&start, buffer + PACKET_REQUEST_START_OFFSET, sizeof start);
        memcpy(&end,   buffer + PACKET_REQUEST_END_OFFSET,   sizeof end);
        start = be64toh(start);
        end   = be64toh(end);
        
        
        // calculations
        uint64_t answer;

        if (!reverse_hash(buffer, start, end, &answer)) {
            fprintf(stderr, "no match found\n");
            close(new_request);
            continue;
        }

        answer = htobe64(answer);

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

bool reverse_hash(const uint8_t *buf, uint64_t start, uint64_t end, uint64_t *answer) {
    for (uint64_t x = start; x < end; x++) {
        uint64_t le = htole64(x);
        uint8_t hash[SHA256_DIGEST_LENGTH];
        lonesha256(hash, (const unsigned char *)&le, sizeof le);

        if (memcmp(buf + PACKET_REQUEST_HASH_OFFSET, hash, SHA256_DIGEST_LENGTH) == 0) {
            *answer = x;
            return true;
        }
    }
    return false;
}