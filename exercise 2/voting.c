/*
 * voting.c
 *
 * Name: Lingesh Kumaar
 * ID: 2018B4A70857P
 *
 * Usage:
 * make
 * ./voting n
 *
 * Process creates n children and simulates a voting scenario
 * Each child casts a binary vote and the parent accumulates the votes
 * and prints the majority decision.
 * One System V message queue is used for this communication.
 *
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include "utils.h"

#define RED   "\x1B[31m"
#define YEL   "\x1B[33m"
#define GRN   "\x1B[32m"
#define RESET "\x1B[0m"

#define MSQ_PERMS   S_IRUSR | S_IWUSR
#define SLEEP_TIME  1
#define MAX_MTEXT   1024
#define NUM_ELEM(x) (sizeof(x) / sizeof((x)[0]))

static int n, msqid;
static pid_t parent_pid;

struct msgbuf {
    long mtype;
    char mtext[MAX_MTEXT];
};

// handle SIGINT and SIGTERM
static void interrupt_handler(int sig) {
    if (getpid() != parent_pid)
        exit(EXIT_SUCCESS);

    // wait for all children to terminate
    while (wait(NULL) > 0)
        continue;

    printf(RED "\nCaught interrupt. Deleting message queue and exiting" RESET
               "\n");
    delete_msgq(msqid);
    exit(EXIT_SUCCESS);
}

void parent_loop(int *children_pids, const uint m) {
    struct msgbuf msg;
    size_t sz = 0;
    int flags = 0;

    for (;;) {
        // send msg to each child
        for (uint i = 0; i < m; i++) {
            msg.mtype = children_pids[i];
            if (msgsnd(msqid, &msg, sz, flags) == -1) {
                delete_msgq(msqid);
                exit_msg("Could not send message. Deleting message queue and "
                         "exiting\n");
            }
        }

        // get replies from all children
        long msgtype     = parent_pid;
        uint count_vote0 = 0, count_vote1 = 0;
        for (uint i = 0; i < n; i++) {
            size_t msglen = msgrcv(msqid, &msg, MAX_MTEXT, msgtype, flags);
            if (msglen == -1) {
                delete_msgq(msqid);
                exit_msg(RED "Parent process - error while receiving message. "
                             "Deleting message queue and exiting" RESET);
            }

            printf("%s\n", msg.mtext);
            if (msg.mtext[msglen - 1] == '1')
                count_vote1++;
            else
                count_vote0++;
        }

        printf("\nCount 0s: %d\tCount 1s: %d\nDecision: ", count_vote0,
               count_vote1);

        // decision
        if (count_vote1 > count_vote0)
            printf(GRN "ACCEPT" RESET "\n");
        else if (count_vote1 < count_vote0)
            printf(RED "REJECT" RESET "\n");
        else
            printf(YEL "TIE" RESET "\n");

        printf("\n");
        sleep(SLEEP_TIME);
    }
}

void child_loop() {
    struct msgbuf msg;
    long msgtype = getpid();
    int flags    = 0;

    // all children need different seeds
    // https://stackoverflow.com/a/8623196
    srand(time(NULL) ^ (getpid() << 16));

    for (;;) {
        // check for message from parent
        size_t msglen = msgrcv(msqid, &msg, MAX_MTEXT, msgtype, flags);
        if (msglen == -1) {
            printf(RED "Child process: %d - error while receiving message. "
                       "Continuing..." RESET "\n",
                   getpid());
        }

        // reply back with 0 or 1
        else {
            int vote  = rand() % 2;
            msg.mtype = parent_pid;
            size_t msgsize =
                snprintf(msg.mtext, MAX_MTEXT, "Child %d replies with: %d",
                         getpid(), vote);
            if (msgsnd(msqid, &msg, msgsize, flags) == -1) {
                printf(RED "Child process: %d - error while sending message. "
                           "Continuing..." RESET "\n",
                       getpid());
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2)
        exit_msg("Usage: ./voting n\nn - number of children");

    n = (int)strtol(argv[1], (char **)NULL, 10);
    if (n <= 0)
        exit_msg("Invalid input. Please enter a positive integer");

    // handle interrupts
    struct sigaction sa;
    sa.sa_flags       = SA_RESTART;
    sa.sa_handler     = interrupt_handler;
    static int sigs[] = {SIGINT, SIGTERM};

    uint m = NUM_ELEM(sigs);
    for (uint i = 0; i < m; i++) {
        if (sigaction(sigs[i], &sa, NULL) == -1)
            exit_msg("Failed to setup interrupt handler");
    }

    // create a unique message queue that will be visible
    // to parent and all its children
    msqid = msgget(IPC_PRIVATE, MSQ_PERMS);
    if (msqid == -1)
        exit_msg("Failed to create message queue.");

    // create children
    printf("Creating %d children...\n\n", n);
    sleep(SLEEP_TIME);
    parent_pid = getpid();
    pid_t children_pids[n];
    for (uint i = 0; i < n; i++) {
        pid_t p = create_fork();
        if (p == 0)
            break;
        children_pids[i] = p;
    }

    // main loops simulating the voting
    if (getpid() == parent_pid)
        parent_loop(children_pids, n);
    else
        child_loop();
}
