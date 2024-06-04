#include "utils.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

ssize_t recv_bytes(int fd, void *buf, size_t buf_len)
{
    ssize_t rc, bytec = 0;

    do {
        rc = recv(fd, buf + bytec, buf_len - bytec, 0);
        DIE(rc < 0, "Error: unable to receive data over connection");

        bytec += rc;
    } while (rc != 0 && (size_t) bytec < buf_len);

    return bytec;
}

ssize_t send_bytes(int fd, void *buf, size_t buf_len)
{
    ssize_t rc, bytec = 0;

    do {
        rc = send(fd, buf + bytec, buf_len - bytec, 0);
        DIE(rc < 0, "Error: unable to send data over connection");

        bytec += rc;
    } while (rc != 0 && (size_t) bytec < buf_len);

    return bytec;
}

void remove_connection(int epoll, int sockfd)
{
    int rc;

    EPOLL_CTL(epoll, EPOLL_CTL_DEL, sockfd, NULL, 0);

    rc = close(sockfd);
    DIE(rc < 0, "Error: unable to close connection");
}
