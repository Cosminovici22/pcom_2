#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "queue.h"
#include "utils.h"

#define MAX_DATAGRAM_LEN 1551
#define IS_FD(EPOLL_DATA) (EPOLL_DATA.u64 < (uint64_t) getdtablesize())

struct client {
    int sockfd;
    char id[MAX_ID_LEN];
    struct queue *subs;
};

struct client *create_client(int sockfd, char id[])
{
    struct client *cl;

    cl = malloc(sizeof *cl);
    DIE(cl == NULL, "Error: unable to allocate memeory");

    cl->sockfd = sockfd;
    strncpy(cl->id, id, MAX_ID_LEN);
    cl->subs = create_queue();

    return cl;
}

int init_udp_sockfd(in_port_t port, in_addr_t addr)
{
    int rc, udp_sockfd;
    struct sockaddr_in servaddr = {0};

    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "Error: unable to initialize UDP socket");

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = addr;
    rc = bind(udp_sockfd, (struct sockaddr *) &servaddr, sizeof servaddr);
    DIE(rc < 0, "Error: unable to bind UDP socket");

    return udp_sockfd;
}

int init_tcp_listenfd(in_port_t port, in_addr_t addr, int backlog)
{
    int rc, tcp_listenfd, aux;
    struct sockaddr_in servaddr = {0};

    tcp_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_listenfd < 0, "Error unable to initialize passive TCP socket");
    rc = setsockopt(tcp_listenfd, SOL_TCP, TCP_NODELAY, &aux, sizeof aux);
    DIE(rc < 0, "Error: unable to set socket options");

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = addr;
    rc = bind(tcp_listenfd, (struct sockaddr *) &servaddr, sizeof servaddr);
    DIE(rc < 0, "Error: unable to bind passive TCP socket");

    rc = listen(tcp_listenfd, backlog);
    DIE(rc < 0, "Error: unable to mark TCP socket as passive");

    return tcp_listenfd;
}

void accept_client(int epoll, int tcp_listenfd)
{
    int connfd;

    connfd = accept(tcp_listenfd, NULL, NULL);
    if (connfd < 0) {
        fprintf(stderr, "Error: unable to establish connection with client");
        return;
    }

    EPOLL_CTL(epoll, EPOLL_CTL_ADD, connfd, NULL, EPOLLIN);
}

int topics_match(char *sub, char *topic)
{
    char subcpy[MAX_TOPIC_LEN + 1], topcpy[MAX_TOPIC_LEN + 1];
    char *lvlsub, *lvltop, *saveptrsub = NULL, *saveptrtop = NULL;
    int any = 0;

    strcpy(subcpy, sub);
    strcpy(topcpy, topic);

    lvlsub = strtok_r(subcpy, "/", &saveptrsub);
    lvltop = strtok_r(topcpy, "/", &saveptrtop);
    while (lvlsub != NULL && lvltop != NULL)
        if (strcmp(lvlsub, lvltop) == 0 || strcmp(lvlsub, "+") == 0) {
            lvlsub = strtok_r(NULL, "/", &saveptrsub);
            lvltop = strtok_r(NULL, "/", &saveptrtop);
            any = 0;
        } else if (strcmp(lvlsub, "*") == 0) {
            lvlsub = strtok_r(NULL, "/", &saveptrsub);
            any = 1;
        } else if (any == 0) {
            return 0;
        } else {
            lvltop = strtok_r(NULL, "/", &saveptrtop);
        }

    return lvlsub == NULL && (lvltop == NULL || any == 1);
}

void forward_message(int udp_sockfd, struct queue *clients)
{
    ssize_t rc;
    char data[MAX_DATAGRAM_LEN];
    struct message_hdr header;
    struct message *msg = (void *) data;
    struct sockaddr_in srcaddr;
    socklen_t srcaddr_len = sizeof srcaddr;

    rc = recvfrom(udp_sockfd, msg, MAX_DATAGRAM_LEN, 0,
        (struct sockaddr *) &srcaddr, &srcaddr_len);
    if(rc < 0) {
        fprintf(stderr, "Error: unable to receive datagram");
        return;
    }
    header.data_len = rc;
    header.src_addr = srcaddr.sin_addr.s_addr;
    header.src_port = srcaddr.sin_port;

    for (struct node *i = clients->head; i != NULL; i = i->next) {
        struct client *cl = i->data;

        if (cl->sockfd == -1)
            continue;

        for (struct node *j = cl->subs->head; j != NULL; j = j->next)
            if (topics_match(j->data, msg->topic)) {
                int sockfd = cl->sockfd;

                send_bytes(sockfd, &header, sizeof header);
                send_bytes(sockfd, msg, header.data_len);
                break;
            }
    }
}

void disconnect_client(int epoll, struct client *cl)
{
    remove_connection(epoll, cl->sockfd);
    cl->sockfd = -1;

    printf("Client %.10s disconnected.\n", cl->id);
}

void handle_message(int epoll, epoll_data_t epoll_data, struct queue *clients)
{
    struct client *cl = IS_FD(epoll_data) ? NULL : epoll_data.ptr;
    int sockfd = cl == NULL ? epoll_data.fd : cl->sockfd;
    struct request_hdr header;
    ssize_t bytec;

    bytec = recv_bytes(sockfd, &header, sizeof header);
    if (bytec == 0) {
        disconnect_client(epoll, cl);
        return;
    }

    if (IS_FD(epoll_data)) {
        int rc;
        char id[MAX_ID_LEN] = {0};
        struct sockaddr_in srcaddr;
        socklen_t srcaddr_len = sizeof srcaddr;

        if (header.data_len > MAX_ID_LEN) {
            fprintf(stderr, "Error: ID exceedes maximum length");
            return;
        }

        recv_bytes(sockfd, id, header.data_len);
        for (struct node *i = clients->head; i != NULL; i = i->next) {
            struct client *aux =  i->data;

            if (strncmp(aux->id, id, MAX_ID_LEN) == 0) {
                if (aux->sockfd < 0) {
                    cl = aux;
                    cl->sockfd = sockfd;
                    break;
                } else {
                    remove_connection(epoll, sockfd);
                    printf("Client %.10s already connected.\n", id);
                    return;
                }
            }
        }

        if (cl == NULL) {
            cl = create_client(sockfd, id);
            enqueue(clients, cl);
        }
        EPOLL_CTL(epoll, EPOLL_CTL_MOD, sockfd, cl, EPOLLIN);

        rc = getpeername(sockfd, (struct sockaddr *) &srcaddr, &srcaddr_len);
        if (rc < 0) {
            fprintf(stderr, "Error: unable to fetch connection address");
            return;
        }

        printf("New client %s connected from %.10s:%hu.\n", cl->id,
            inet_ntoa(srcaddr.sin_addr), ntohs(srcaddr.sin_port));
    } else {
        char *data;

        if (header.data_len > MAX_TOPIC_LEN) {
            fprintf(stderr, "Error: topic name exceedes maximum length");
            return;
        }

        data = calloc(header.data_len + 1, sizeof(char));
        DIE(data == NULL, "Errro: unable to allocate memeory");
        recv_bytes(cl->sockfd, data, header.data_len);

        switch (header.cmd) {
        case CMD_SUBSCRIBE:
            enqueue(cl->subs, data);
            break;
        case CMD_UNSUBSCRIBE:
            for (struct node *i = cl->subs->head; i != NULL; i = i->next)
                if (topics_match(i->data, data)) {
                    queue_remove_node(cl->subs, i);
                    break;
                }
            free(data);
            break;
        default:
            fprintf(stderr, "Error: invalid command requested");
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    int rc, udp_sockfd, tcp_listenfd, epoll;
    struct queue *clients;
    struct client *cl;
    in_port_t port;

    DIE(argc != 2, "Error: invalid number of arguments");
    rc = sscanf(argv[1], "%hu", &port);
    DIE(rc == 0, "Error: invalid port");

    setbuf(stdout, NULL);

    udp_sockfd = init_udp_sockfd(port, INADDR_ANY);
    tcp_listenfd = init_tcp_listenfd(port, INADDR_ANY, 128);

    clients = create_queue();
    epoll = epoll_create1(0);
    DIE(rc < 0, "Error: unable to initialize I/O multiplexing facility");
    EPOLL_CTL(epoll, EPOLL_CTL_ADD, STDIN_FILENO, NULL, EPOLLIN);
    EPOLL_CTL(epoll, EPOLL_CTL_ADD, tcp_listenfd, NULL, EPOLLIN);
    EPOLL_CTL(epoll, EPOLL_CTL_ADD, udp_sockfd, NULL, EPOLLIN);

    while (1) {
        struct epoll_event event;

        rc = epoll_wait(epoll, &event, 1, -1);
        if (rc < 0)
            continue;

        if (event.data.fd == STDIN_FILENO) {
            char *cmd = NULL;

            if (scanf("%ms", &cmd) != 1) {
                break;
            } else if (strcmp(cmd, "exit") == 0) {
                free(cmd);
                break;
            } else {
                fprintf(stderr, "Error: invalid command");
            }
            free(cmd);
        } else if (event.data.fd == tcp_listenfd) {
            accept_client(epoll, tcp_listenfd);
        } else if (event.data.fd == udp_sockfd) {
            forward_message(udp_sockfd, clients);
        } else {
            handle_message(epoll, event.data, clients);
        }
    }

    while ((cl = dequeue(clients)) != NULL) {
        void *sub;

        while ((sub = dequeue(cl->subs)) != NULL)
            free(sub);
        if (cl->sockfd > 0)
            remove_connection(epoll, cl->sockfd);
        free(cl->subs);
        free(cl);
    }
    free(clients);

    remove_connection(epoll, udp_sockfd);
    remove_connection(epoll, tcp_listenfd);
    rc = close(epoll);
    DIE(rc < 0, "Error: unable to close I/O multiplexing facility");

    return 0;
}
