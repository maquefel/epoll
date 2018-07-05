#include "connection.h"

#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int make_non_blocking(int fd)
{
    int flags, s;

    flags = fcntl (fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror ("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl (fd, F_SETFL, flags);
    if (s == -1)
    {
        perror ("fcntl");
        return -1;
    }

    return 0;
}

int init_socket(struct in_addr ipaddr, uint16_t port)
{
    struct sockaddr_in addr;
    int on = 1;
    int ret = 0;
    int errsv = 0;

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    errsv = errno;
    if (ret == -1)
    {
        switch (errsv)
        {
            case EPROTONOSUPPORT:
                break;
            case EMFILE:
                break;
            case ENFILE:
                break;
            case EACCES:
                break;
            case ENOBUFS:
                break;
            case EINVAL:
                break;
            default:
                break;
        }
        fprintf(stderr, "socket open failure with [%d] : %s\n", errsv, strerror(errsv));
        goto fail;
    }

    ret = setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    errsv = errno;

    if(ret == -1) {
        fprintf(stderr, "setsockopt SO_REUSEADDR failure with [%d] : %s\n", errsv, strerror(errsv));
    }

    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = ipaddr.s_addr;

    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        errsv = errno;
        fprintf(stderr, "ERROR on binding %s:%d", inet_ntoa(addr.sin_addr), port);
        goto fail;
    }

    return sockfd;

    fail:
    errno = errsv;
    return -1;
}

int accept_tcp_client(int fd)
{
    int newsockfd;
    int optval = 1;
    socklen_t clilen;
    struct sockaddr_in cli_addr;
    int errsv = 0;

    clilen = sizeof(cli_addr);

    newsockfd = accept(fd, (struct sockaddr *) &cli_addr, &clilen);
    errsv = errno;

    // EAGAIN EWOULDBLOCK
    if (newsockfd < 0)
    {
        errno = errsv;
        goto quit;
    }

    socklen_t optlen = sizeof(optval);

    if(setsockopt(newsockfd, IPPROTO_TCP, O_NDELAY, &optval, optlen) < 0) {
        errsv = errno;
        close(newsockfd);
        goto quit;
    }

    fprintf(stderr, "Client connected: %s:%d\n", inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);

    return newsockfd;

    quit:
    errno = errsv;
    return -1;
}

int connect_tcp_server(struct in_addr ip, uint16_t port)
{
    int errsv = 0;
    int sockfd = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    //make_non_blocking(sockfd);

    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr = ip;
    serv_addr.sin_port = htons(port);

    fprintf(stderr, "connecting to remote server %s:%d\n", inet_ntoa(ip), port);

    int ret = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    errsv = errno;

    // EINPROGRESS
    if(ret < 0 && errsv != EINPROGRESS) {
        fprintf(stderr, "failed connecting with [%d] : %s\n", errsv, strerror(errsv));
        goto fail;
    }

    return sockfd;

    fail:
    errno = errsv;
    return -1;
}
