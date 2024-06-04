#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils.h"

int init_sockfd(in_port_t servport, in_addr_t addr)
{
    int rc, sockfd, aux;
    struct sockaddr_in servaddr = {0};

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "Error: unable to initialize connection socket");
    rc = setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &aux, sizeof aux);
    DIE(rc < 0, "Error: unable to set socket options");

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(servport);
    servaddr.sin_addr.s_addr = addr;
    rc = connect(sockfd, (struct sockaddr *) &servaddr, sizeof servaddr);
    DIE(rc < 0, "Error: unable to establish connection to server");

    return sockfd;
}

void request_operation(int sockfd, uint8_t cmd, uint8_t data_len, char *data)
{
    struct request_hdr header = {.cmd = cmd, .data_len = data_len};

    send_bytes(sockfd, &header, sizeof header);
    send_bytes(sockfd, data, data_len);
}

void print_message(int sockfd, struct message_hdr header)
{
    struct message *msg;

    msg = malloc(header.data_len);
    DIE(msg == NULL, "Error: unable to allocate memory");

    recv_bytes(sockfd, msg, header.data_len);
    printf(
        "%s:%hu - %.50s - ",
        inet_ntoa((struct in_addr) {header.src_addr}),
        ntohs(header.src_port),
        msg->topic
    );
    switch (msg->content_type) {
    case 0:
        uint32_t n;

        n = ntohl(*(uint32_t *) (msg->content + 1));
        printf("INT - ");
        if ((uint8_t) msg->content[0] == 1 && n != 0)
            printf("-");
        printf("%u\n", n);
        break;
    case 1:
        printf(
            "SHORT_REAL - %.2f\n",
            ntohs(*(uint16_t *) msg->content) / 100.0
        );
        break;
    case 2:
        char format[16];
        uint32_t x, pow = 1;

        x = ntohl(*(uint32_t *) (msg->content + 1));
        for (int i = 0; i < (uint8_t) msg->content[5]; i++)
            pow *= 10;
        sprintf(format, "%%.%hhuf\n", (uint8_t) msg->content[5]);

        printf("FLOAT - ");
        if ((uint8_t) msg->content[0] == 1 && x != 0)
            printf("-");
        printf(format, (float) x / pow);
        break;
    case 3:
        printf("STRING - %.1500s\n", msg->content);
        break;
    default:
        printf("Error: Invalid message content type");
        break;
    }

    free(msg);
}

int main(int argc, char *argv[])
{
    int rc, sockfd, epoll;
    struct in_addr servaddr;
    in_port_t servport;

    DIE(argc != 4, "Error: invalid number of arguments");
    DIE(strlen(argv[1]) > MAX_ID_LEN, "Error: ID exceedes maximum length");
    rc = inet_aton(argv[2], &servaddr);
    DIE(rc == 0, "Error: invalid address");
    rc = sscanf(argv[3], "%hu", &servport);
    DIE(rc == 0, "Error: invalid port");

    setbuf(stdout, NULL);

    sockfd = init_sockfd(servport, servaddr.s_addr);
    request_operation(sockfd, CMD_NONE, strlen(argv[1]), argv[1]);

    epoll = epoll_create1(0);
    DIE(rc < 0, "Error: unable to initialize I/O multiplexing facility");
    EPOLL_CTL(epoll, EPOLL_CTL_ADD, STDIN_FILENO, NULL, EPOLLIN);
    EPOLL_CTL(epoll, EPOLL_CTL_ADD, sockfd, NULL, EPOLLIN);

    while (1) {
        struct epoll_event event;

        rc = epoll_wait(epoll, &event, 1, -1);
        if (rc < 0)
            continue;

        if (event.data.fd == STDIN_FILENO) {
            char *cmd = NULL, *arg = NULL;

            if (scanf("%ms", &cmd) != 1) {
                fprintf(stderr, "Error: unable to scan input");
                break;
            } else if (strcmp(cmd, "exit") == 0) {
                free(cmd);
                break;
            } else if (strcmp(cmd, "subscribe") == 0
                    && scanf("%*[ ]%m[^\n]", &arg) != 0
                    && strlen(arg) <= MAX_TOPIC_LEN) {
                request_operation(sockfd, CMD_SUBSCRIBE, strlen(arg), arg);
                printf("Subscribed to topic %s\n", arg);
            } else if (strcmp(cmd, "unsubscribe") == 0
                    && scanf("%*[ ]%m[^\n]", &arg) != 0
                    && strlen(arg) <= MAX_TOPIC_LEN) {
                request_operation(sockfd, CMD_UNSUBSCRIBE, strlen(arg), arg);
                printf("Unsubscribed from topic %s\n", arg);
            } else {
                fprintf(stderr, "Error: invalid command");
            }
            free(cmd);
            free(arg);
        } else if (event.data.fd == sockfd) {
            ssize_t bytec;
            struct message_hdr header;

            bytec = recv_bytes(sockfd, &header, sizeof header);
            if (bytec == 0) {
                fprintf(stderr, "Error: connection to server closed");
                break;
            }
            print_message(sockfd, header);
        }
    }

    remove_connection(epoll, sockfd);
    rc = close(epoll);
    DIE(rc < 0, "Error: unable to close I/O multiplexing facility");

    return 0;
}
