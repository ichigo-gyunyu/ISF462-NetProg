#ifndef UTILS_H
#define UTILS_H

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <wait.h>

#define BUFLEN      4096
#define NUM_ELEM(x) (sizeof(x) / sizeof((x)[0]))

void perror_and_exit(const char *msg);

#endif
