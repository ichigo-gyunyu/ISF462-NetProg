#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE // for getaddrinfo_a

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define TMP_BUFLEN 2048

void perror_and_exit(const char *msg);

char *readLine(FILE *fp, char *buf, const int n);

/**
 * Wrapper around getaddrinfo()
 */
struct addrinfo *host_serv(const char *host, const char *serv, int family, int socktype);

/**
 * Wrapper around inet_ntop
 */
char *sock_ntop_host(const struct sockaddr *sa, socklen_t salen);

/**
 * Sets sin_port or sin6_port
 */
void sock_set_port(struct sockaddr *sa, int port);

#endif // UTILS_H
