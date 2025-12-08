<<<<<<< HEAD
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

/**
 * @brief Packs a value of a given bit size into an specific bitoffset of a buffer
* @param Buffer    Array of memory
* @param BufferLen Length of the buffer
* @param Bitoffset Bit offset in the buffer
* @param Value     The value we want to pack
* @param BitSize   Size of the value in bits

*/
fn_internal void PackBits(u8* Buffer, u32 BufferLen, u64* BitOffset, u64 Value, u64 BitSize );


/**
 * @brief Unpacks a value from an specific bitrange inside a buffer
* @param Buffer    Array of memory
* @param BufferLen Length of the buffer
* @param Bitoffset Bit offset in the buffer
* @param BitSize   Size of the value in bits
* @return Value stored
*/
fn_internal u64 UnpackBits(u8* Buffer, u32 BufferLen, u64* BitOffset, u64 BitSize );

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

fn_internal void
PackBits(u8* Buffer, u32 BufferLen, u64* BitOffset, u64 Value, u64 BitSize) {
    for (u64 i = 0; i < BitSize; i += 1) {
        // Calculate the position for the current bit using the loop counter 'i'
        u64 CurrentBit = *BitOffset + i;
        u64 BytePos    = CurrentBit / 8;
        u64 BitPos     = CurrentBit % 8;

        // Check for out-of-bounds write
        if (BytePos >= BufferLen) {
            // Handle error
            return;
        }

        // Extract the bit to write (from MSB to LSB)
        u8 Bit = (u8)((Value >> (BitSize - 1 - i)) & 0x01);

        // Write the bit into the buffer without disturbing other bits
        Buffer[BytePos] = (Buffer[BytePos] & ~(1 << (7 - BitPos))) | (Bit << (7 - BitPos));
    }

    *BitOffset += BitSize;
}

fn_internal u64
UnpackBits(u8* Buffer, u32 BufferLen, u64* BitOffset, u64 BitSize) {
    // Check if the read will go out of bounds
    if (((*BitOffset + BitSize) / 8) >= BufferLen) {
        // Handle error: log, return 0, or assert
        return 0;
    }

    u64 Value = 0;

    for (u64 i = 0; i < BitSize; i += 1) {
        u64 CurrentBit = *BitOffset + i;
        u64 BytePos    = CurrentBit / 8;
        u64 BitPos     = CurrentBit % 8;

        u8 Bit = (Buffer[BytePos] >> (7 - BitPos)) & 0x01;
        Value = (Value << 1) | Bit;
    }

    *BitOffset += BitSize;

    return Value;
}

#endif
=======
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

/**
 * @brief Packs a value of a given bit size into an specific bitoffset of a buffer
* @param Buffer    Array of memory
* @param BufferLen Length of the buffer
* @param Bitoffset Bit offset in the buffer
* @param Value     The value we want to pack
* @param BitSize   Size of the value in bits

*/
fn_internal void PackBits(u8* Buffer, u32 BufferLen, u64* BitOffset, u64 Value, u64 BitSize );


/**
 * @brief Unpacks a value from an specific bitrange inside a buffer
* @param Buffer    Array of memory
* @param BufferLen Length of the buffer
* @param Bitoffset Bit offset in the buffer
* @param BitSize   Size of the value in bits
* @return Value stored
*/
fn_internal u64 UnpackBits(u8* Buffer, u32 BufferLen, u64* BitOffset, u64 BitSize );

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

fn_internal void
PackBits(u8* Buffer, u32 BufferLen, u64* BitOffset, u64 Value, u64 BitSize) {
    for (u64 i = 0; i < BitSize; i += 1) {
        // Calculate the position for the current bit using the loop counter 'i'
        u64 CurrentBit = *BitOffset + i;
        u64 BytePos    = CurrentBit / 8;
        u64 BitPos     = CurrentBit % 8;

        // Check for out-of-bounds write
        if (BytePos >= BufferLen) {
            // Handle error
            return;
        }

        // Extract the bit to write (from MSB to LSB)
        u8 Bit = (u8)((Value >> (BitSize - 1 - i)) & 0x01);

        // Write the bit into the buffer without disturbing other bits
        Buffer[BytePos] = (Buffer[BytePos] & ~(1 << (7 - BitPos))) | (Bit << (7 - BitPos));
    }

    *BitOffset += BitSize;
}

fn_internal u64
UnpackBits(u8* Buffer, u32 BufferLen, u64* BitOffset, u64 BitSize) {
    // Check if the read will go out of bounds
    if (((*BitOffset + BitSize) / 8) >= BufferLen) {
        // Handle error: log, return 0, or assert
        return 0;
    }

    u64 Value = 0;

    for (u64 i = 0; i < BitSize; i += 1) {
        u64 CurrentBit = *BitOffset + i;
        u64 BytePos    = CurrentBit / 8;
        u64 BitPos     = CurrentBit % 8;

        u8 Bit = (Buffer[BytePos] >> (7 - BitPos)) & 0x01;
        Value = (Value << 1) | Bit;
    }

    *BitOffset += BitSize;

    return Value;
}

#endif
>>>>>>> 154fcbcc98d4260859f45c905ed533ba04da06cf
