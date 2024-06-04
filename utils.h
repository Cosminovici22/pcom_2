#ifndef _UTILS_H
#define _UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>

#define MAX_ID_LEN 10
#define MAX_TOPIC_LEN 50

#define CMD_NONE 0
#define CMD_SUBSCRIBE 1
#define CMD_UNSUBSCRIBE 2

#define DIE(assertion, message) \
    do { \
        if (assertion) { \
            fprintf(stderr, "%s\n", message); \
            exit(EXIT_FAILURE); \
        } \
    } while (0);

#define EPOLL_CTL(EPOLL, OP, FD, PTR, EVENTS) \
    do { \
        int rc; \
        struct epoll_event event = {0}; \
        \
        event.events = EVENTS; \
        event.data.fd = FD; \
        if (PTR != NULL) \
            event.data.ptr = PTR; \
        \
        rc = epoll_ctl(EPOLL, OP, FD, &event); \
        DIE(rc < 0, "Error: unable to allocate memeory"); \
    } while (0);

struct __attribute__ ((__packed__)) request_hdr {
    uint8_t cmd;
    uint8_t data_len;
};

struct __attribute__ ((__packed__)) message_hdr {
    uint32_t src_addr;
    uint16_t src_port;
    uint16_t data_len;
};

struct __attribute__ ((__packed__)) message {
    char topic[MAX_TOPIC_LEN];
    uint8_t content_type;
    char content[];
};

ssize_t recv_bytes(int fd, void *buf, size_t buf_len);
ssize_t send_bytes(int fd, void *buf, size_t buf_len);
void remove_connection(int epoll, int sockfd);

#endif
