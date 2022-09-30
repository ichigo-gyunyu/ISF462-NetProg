#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>

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

void delete_msgq(int msqid) {
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
        exit_msg("Failed to delete message queue");
}
