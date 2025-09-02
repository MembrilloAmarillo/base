#ifndef _MY_NET_H_
#define _MY_NET_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"

#define UDP_PORT_SERVER 8085
#define UDP_PORT_CLIENT 8080

#define UDP_BUFSIZE  1024

typedef struct udp_socket udp_socket;
struct udp_socket {
    int sockfd;
    int cli_sockfd;
    char buffer[UDP_BUFSIZE];
    char cli_buffer[UDP_BUFSIZE];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t cli_len;
    socklen_t ser_len;
};

typedef struct udp_socket_bind udp_socket_bind;
struct udp_socket_bind {
    int sockfd;
    char buffer[UDP_BUFSIZE];
    struct sockaddr_in server_addr;
    socklen_t len;
};

typedef struct udp_socket_connect udp_socket_connect;
struct udp_socket_connect {
    int sockfd;
    char buffer[UDP_BUFSIZE];
    struct sockaddr_in client_addr;
    socklen_t len;
};

/**
 * @brief Function that creates two sockets, one for receiving from YAMCS
 * and another for sending to YAMCS
 * @return udp_socket two ports and two sockets initialized
 */
udp_socket udp_socket_init();

/**
 * @brief Function that creates a socket and binds it to the sockdf 
 * @param U32 Port number you want to bind 
 *
 * @return udp_socket_bind The structure with the data for that connection
 */
udp_socket_bind UDP_CreateSocketBind( U32 PORT );

/**
 * @brief Function that connects to a binded socket 
 * @param const char* IP of the socket you want to connect to
 * @param U32         Port number you want to connect
 *
 * @return udp_socket_connect The structure with the data for that connection
 */
udp_socket_connect UDP_CreateSocketConnect( const char* IP, U32 PORT );

#endif

#ifdef MY_NET_IMPL
udp_socket udp_socket_init() {
    udp_socket sock;
    sock.cli_len = UDP_BUFSIZE; //sizeof(sock.client_addr);
    sock.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if( sock.sockfd < 0 ) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&sock.server_addr, 0, sizeof(sock.server_addr));
    sock.server_addr.sin_family      = AF_INET;
    sock.server_addr.sin_port        = htons(UDP_PORT_SERVER);

#if 1
    if (bind(sock.sockfd, (struct sockaddr *)&sock.server_addr, sizeof(sock.server_addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
#else
    inet_pton(AF_INET, SERVER_IP, &sock.server_addr.sin_addr);
    connect(sock.sockfd, (struct sockaddr *)&sock.server_addr, sizeof(sock.server_addr));
#endif

    sock.ser_len = UDP_BUFSIZE;//sizeof(sock.server_addr);
    /* 1. Create socket */
    sock.cli_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock.cli_sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* 2. Fill server address */
    memset(&sock.client_addr, 0, sizeof(sock.client_addr));
    sock.client_addr.sin_family = AF_INET;
    sock.client_addr.sin_port   = htons(UDP_PORT_CLIENT);

#if 0
    if (bind(sock.cli_sockfd, (struct sockaddr *)&sock.client_addr, sizeof(sock.client_addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
#else
    inet_pton(AF_INET, SERVER_IP, &sock.client_addr.sin_addr);
    connect(sock.cli_sockfd, (struct sockaddr *)&sock.client_addr, sizeof(sock.client_addr));
#endif

    return sock;
}

udp_socket_bind UDP_CreateSocketBind( U32 PORT ) {
    udp_socket_bind sock;
    sock.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if( sock.sockfd < 0 ) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&sock.server_addr, 0, sizeof(sock.server_addr));
    sock.server_addr.sin_family      = AF_INET;
    sock.server_addr.sin_port        = htons(PORT);

    if (bind(sock.sockfd, (struct sockaddr *)&sock.server_addr, sizeof(sock.server_addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }

    sock.len = UDP_BUFSIZE;//sizeof(sock.server_addr);

    return sock;
}

udp_socket_connect UDP_CreateSocketConnect( const char* IP, U32 PORT ) {
    udp_socket_connect sock;
    sock.len = UDP_BUFSIZE;
    /* 1. Create socket */
    sock.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock.sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* 2. Fill server address */
    memset(&sock.client_addr, 0, sizeof(sock.client_addr));
    sock.client_addr.sin_family = AF_INET;
    sock.client_addr.sin_port   = htons(PORT);

    inet_pton(AF_INET, IP, &sock.client_addr.sin_addr);
    connect(sock.sockfd, (struct sockaddr *)&sock.client_addr, sizeof(sock.client_addr));

    return sock;
}

#endif