#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>

struct in_addr ip = {0};
uint16_t port = 0;
int server_fd;
int thunder_flag = 0;

static struct option long_options[] =
{
    {"ip",              required_argument,  0,                  'i'},
    {"port",            required_argument,  0,                  'p'},
    {"threads",         required_argument,  0,                  'n'},
    {"thunder",         no_argument,        0,                  't'},
    {"help",            no_argument,        0,                  'h'},
    {0, 0, 0, 0}
};

void PrintUsage(int argc, char *argv[]) {
    argv = argv;
    if(argc >=1) {
        printf(
            "-i, --ip=ADDR              specify ip address\n" \
            "-p, --port=PORT            specify port\n" \
            "-n, --threads=NUM          specify number of threads to use\n" \
            "-t, --thunder              not adding EPOLLEXCLUSIVE\n"
            "-h, --help                 prints this message\n"
        );
    }
}

int init_socket(struct in_addr ipaddr, uint16_t port);

void *listen_thread(void *arg);
void *connect_thread(void *arg);

void loop(int n);

int connect_tcp_server(struct in_addr ip, uint16_t port);

int make_non_blocking(int fd);

int main(int argc, char const *argv[]) {
    int c = -1;
    int option_index = 0;

    char *ip_addr = 0;
    unsigned int num_threads = 0;

    while((c = getopt_long(argc, argv, "i:p:n:th", long_options, &option_index)) != -1) {
        switch(c) {
            case 'i':
                ip_addr = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'n':
                num_threads = atoi(optarg);
                break;
            case 't':
                thunder_flag = 1;
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

    server_fd = init_socket(ip, port);

    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("couldn't start listening");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if(make_non_blocking(server_fd) != 0) {
        perror("make_non_blocking");
        exit(EXIT_FAILURE);
    }

    loop(num_threads);

    sleep(1);

    return 0;
}

void loop(int n)
{
    pthread_t server_threads[n];
    pthread_t client_threads[n + n + n + n];

    for(size_t i = 0; i < n; i++)
        if (pthread_create(server_threads + i, NULL, listen_thread, (void *)i))
            perror("couldn't initiate server thread"), exit(-1);

        for(size_t i = 0; i < (n + n + n + n); i++) {
            if(thunder_flag)
                usleep(1000000);
            if (pthread_create(client_threads + i, NULL, connect_thread, (void *)i))
                perror("couldn't initiate client thread"), exit(-1);
        }

        for(size_t i = 0; i < n; i++)
            pthread_join(server_threads[i], NULL);
}


void *listen_thread(void *arg)
{
    int epoll_fd;
    ssize_t ec;
    epoll_fd = epoll_create1(0);

    if (epoll_fd < 0) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    char buffer[1024] = {0};

    sprintf(buffer, "Hello World - from server thread %lu", (size_t)arg);

    struct epoll_event event = {0};
    event.data.ptr = (void *)((uintptr_t)server_fd);
    event.events = EPOLLIN | EPOLLERR;

    if(!thunder_flag)
        event.events |= EPOLLEXCLUSIVE;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) != 0) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    while(1) {
        struct epoll_event events[10];
        ec = epoll_wait(epoll_fd, events, 10, -1);

        if (ec <= 0) {
            fprintf(stderr, "Server thread %lu wakeup no events / error\n",
                    (size_t)arg);
            perror("errno ");
            return NULL;
        }

        fprintf(stderr, "Server thread %lu woke up with %lu events\n",
                (size_t)arg, ec);

        for(int i = 0; i < ec; i++) {
            int connfd;
            struct sockaddr_storage client_addr;
            socklen_t client_addrlen = sizeof client_addr;

            /* accept up all connections. we're non-blocking, -1 == no more connections */
            //int processed_conns = 0;
            if(events[0].events & EPOLLIN) {
                if((connfd = accept(server_fd,
                    (struct sockaddr *)&client_addr,
                                    &client_addrlen)) >= 0)
                {
                    fprintf(stderr,
                            "Server thread %lu accepted a connection and saying hello.\n",
                            (size_t)arg);

                    if (write(connfd, buffer, strlen(buffer)) < strlen(buffer))
                        perror("server write failed");

                    close(connfd);
                    sleep(1);
                } else if(errno == EAGAIN) {
                    fprintf(stderr, "Server thread %lu ***FAILED*** to proccess accept with EAGAIN\n", (size_t)arg);
                }
            }
        }
    }

    close(epoll_fd);
    return NULL;
}

void *connect_thread(void *arg)
{
    int errsv = 0;
    int fd = connect_tcp_server(ip, port);
    errsv = errno;
    if(fd == -1) {
        perror("connect_tcp_server");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "client number %lu connected\n", (size_t)arg);
    char buffer[128] = {0};
    int len = 0;

    len = read(fd, buffer, 128);

    fprintf(stderr, "client %lu: %s\n", (size_t)arg, buffer);

    close(fd);

    return NULL;
}
