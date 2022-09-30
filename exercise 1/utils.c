#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

void exit_msg(char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

pid_t create_fork() {
    pid_t p = fork();
    if (p == -1) {
        exit_msg("Failed to create fork");
    }
    return p;
}

void log_entry(pid_t p, char *msg) {
    char logfile[20];
    snprintf(logfile, sizeof(logfile), "logs/%d.log", p);
    int fd = open(logfile, O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd == -1)
        exit_msg("Could not open file for logging");

    write(fd, msg, strlen(msg));
}
