#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// for printing colours
#define RED "\x1B[31m"
#define YEL "\x1B[33m"
#define GRN "\x1B[32m"
#define BLU "\x1B[34m"
#define RST "\x1B[0m"

#define TMP_BUFLEN 512

#define NUM_ELEM(x) (sizeof(x) / sizeof((x)[0]))

// for sending messages to/from the broker
struct msg {
    char topic[TMP_BUFLEN];
    char msg[TMP_BUFLEN];
};

void  perror_and_exit(const char *msg);
char *readLine(FILE *fp, char *buf, const int n);
void  flushstdin();

#endif // UTILS_H
