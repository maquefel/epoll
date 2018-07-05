#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <stdint.h>
#include <netinet/ip.h>

int make_non_blocking(int fd);
int init_socket(struct in_addr ipaddr, uint16_t port);
int accept_tcp_client(int fd);
int connect_tcp_server(struct in_addr ip, uint16_t port);

#endif
