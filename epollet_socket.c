// C program to demonstrate epollet via socket
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

#include "connection.h"

struct in_addr ip = {0};
uint16_t port = 0;
int server_fd;
int random_fd;
int null_fd;
int level_flag = 0;

static struct option long_options[] =
{
    {"ip",              required_argument,  0,                  'i'},
    {"port",            required_argument,  0,                  'p'},
    {"level",           no_argument,        0,                  'l'},
    {"help",            no_argument,        0,                  'h'},
    {0, 0, 0, 0}
};

void PrintUsage(int argc, char *argv[]) {
    argv = argv;
    if(argc >=1) {
        printf(
            "-i, --ip=ADDR              specify ip address\n" \
            "-p, --port=PORT            specify port\n" \
            "-l, --level                not adding EPOLLET\n"
            "-h, --help                 prints this message\n"
        );
    }
}

int main(int argc, char const *argv[])
{
    pid_t p;

    const char chunk1[] = "first chunk of data";
    const char chunk2[] = "second chunk of data";
    char buffer[1024];

    int c = -1;
    int option_index = 0;

    char *ip_addr = 0;

    while((c = getopt_long(argc, argv, "i:p:lh", long_options, &option_index)) != -1) {
        switch(c) {
            case 'i':
                ip_addr = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'l':
                level_flag = 1;
                break;
            case 'h':
                PrintUsage(argc, argv);
                exit(EXIT_SUCCESS);
                break;
            default:
                break;
        }
    }

    int ret = inet_pton(AF_INET, ip_addr, &ip);

    if(ret != 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    p = fork();

    if (p < 0)
    {
        fprintf(stderr, "fork Failed" );
        return 1;
    }
    // parent process will block on epollwait waiting for EPOLLIN
    else if (p > 0)
    {
        server_fd = init_socket(ip, port);

        if (listen(server_fd, SOMAXCONN) < 0) {
            perror("couldn't start listening");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        int connfd;
        struct sockaddr_storage client_addr;
        socklen_t client_addrlen = sizeof client_addr;

        connfd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addrlen);

        if(connfd == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        make_non_blocking(connfd);

        int efd = epoll_create1 (0);
        struct epoll_event event;
        struct epoll_event events[1];

        event.data.fd = connfd;
        event.events = EPOLLIN;

        if(!level_flag)
            event.events |= EPOLLET;

        int ret = epoll_ctl (efd, EPOLL_CTL_ADD, connfd, &event);

        while (1) {
          int n = epoll_wait (efd, events, 1, -1);

          if(n > 0) {
            printf("event from pipe arrived : ");

            if(events[0].events & EPOLLERR)
              printf("EPOLLERR ");

            if(events[0].events & EPOLLHUP)
              printf("EPOLLHUP ");

            if(events[0].events & EPOLLIN) {
              static int epoll_count;
              printf("EPOLLIN : %d", epoll_count++);
            }
            printf("\n");

            int len = 0;

            if(!level_flag)
                read(connfd, buffer, 50);

            if(level_flag)
                sleep(2);

            if(len > 0)
                printf("readed : %d bytes\n", len);
          }
       }
    }
    // child process will send chunk1 wait and send chunk2
    else
    {
        int errsv = 0;

        random_fd = open("/dev/urandom", O_WRONLY);
        errsv = errno;
        if(random_fd == -1) {
            perror("/dev/urandom");
            exit(EXIT_FAILURE);
        }

        int fd = connect_tcp_server(ip, port);
        errsv = errno;
        if(fd == -1) {
            perror("connect_tcp_server");
            exit(EXIT_FAILURE);
        }

        read(random_fd, buffer, 256);

        // send some bytes
        write(fd, buffer, 50);

        // sleep a bit so epoll_wait will unblock and server will read a bit
        usleep(1000000);

        // and send some moreeee one byte
        write(fd, buffer, 1);

        // wait for sig
        pause();
        close(fd);
    }

    return 0;
}
