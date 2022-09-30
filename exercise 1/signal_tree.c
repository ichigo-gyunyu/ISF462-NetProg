/*
 * signal_tree.c
 *
 * Name: Lingesh Kumaar
 * ID: 2018B4A70857P
 *
 * Usage:
 * make
 * ./signal_tree N A S
 *
 * To view per process log files, compile with DEBUG flag:
 * make debug
 */

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef DEBUG
#include <fcntl.h>
#endif

#include "utils.h"

#define RED   "\x1B[31m"
#define YEL   "\x1B[33m"
#define GRN   "\x1B[32m"
#define RESET "\x1B[0m"

#define SLEEP_TIME 1

static volatile int is_zero     = 0;
static volatile int cnt_signals = 0;

static int n, a, s, depth, points;

static void siginfo_handler(int sig, siginfo_t *si, void *ucontext) {

    if (sig == SIGINT || sig == SIGTERM) {
        _exit(EXIT_FAILURE); // do not flush the buffer
    }

    if (is_zero) {
        return;
    }

    cnt_signals++;
    pid_t me       = getpid();
    pid_t from     = si->si_pid;
    int depth_from = si->si_value.sival_int;
    char *relation;
    char update[30];

    // update points
    if (depth > depth_from) {
        relation = "Parent";
        snprintf(update, sizeof(update), "Add %d points", a);
        points += a;
    } else if (depth < depth_from) {
        relation = "Child";
        snprintf(update, sizeof(update), "Remove %d points", s);
        points -= s;
    } else {
        relation = "Sibling";
        snprintf(update, sizeof(update), "Remove %d points", s / 2);
        points -= s / 2;
    }

    // display information
    char output[200];
    snprintf(output, sizeof(output),
             RESET "Signal from %d(depth %d) to %d(depth %d). "
                   "Relation: %s. %s. Updated Points: %d\n",
             from, depth_from, me, depth, relation, update, points);
    printf(RESET "%s", output);
    fflush(stdout);

#ifdef DEBUG
    char msg[100];
    snprintf(msg, sizeof(msg),
             "Points Update: %d. (Received signal from %d, relationship %s.)\n",
             points, from, relation);
    log_entry(me, msg);
#endif

    // don't update points once it reaches 0
    if (points <= 0)
        is_zero = 1;

    sleep(SLEEP_TIME);
    return;
}

void process_tree(int current, int *depth) {

    int child_left  = 2 * current;
    int child_right = child_left + 1;

    // recursively spawn more processes
    if (child_left <= n + 1) {
        pid_t l = create_fork();
        if (l) {
            if (child_right <= n + 1) {
                pid_t r = create_fork();
                if (!r) {
                    *depth = *depth + 1;
                    process_tree(child_right, depth);
                }
            }
        } else {
            *depth = *depth + 1;
            process_tree(child_left, depth);
        }
    }
}

int main(int argc, char **argv) {

    if (argc != 4) {
        exit_msg("3 arguements required for N, A and S\n");
    }

    n = (int)strtol(argv[1], (char **)NULL, 10);
    a = (int)strtol(argv[2], (char **)NULL, 10);
    s = (int)strtol(argv[3], (char **)NULL, 10);

    depth  = 1; // position in the process tree
    points = n;

    pid_t root = getpid();
    printf("Info about all the processes created:\n");

    // setup the handler
    struct sigaction sa;
    sa.sa_sigaction = siginfo_handler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;
    sigaction(SIGRTMIN, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sigset_t block_set, old_set;
    sigaddset(&block_set, SIGRTMIN);
    if (sigprocmask(SIG_BLOCK, &block_set, &old_set) == -1)
        exit_msg("Error while blocking signal");

    // create the binary tree of processes
    process_tree(1, &depth);

    // information that will be passed using a signal
    union sigval sv = {.sival_int = depth};

    // prepare send signals
    pid_t me     = getpid();
    pid_t parent = getppid();

    // list of all the forked processes
    printf("Process PID %d. Parent PID %d. Depth %d\n", me, parent, depth);
    sleep(SLEEP_TIME);
    if (me == root) {
        printf("\nPreparing to send signals...\n\n");
    }
    fflush(stdout);
    sleep(SLEEP_TIME);

    for (pid_t i = parent - n; i <= me + n + 1; i++) {
        // send signals to only those processes
        // in the porcess tree
        if (i <= root)
            continue;

        sigqueue(i, SIGRTMIN, sv);
    }

    if (sigprocmask(SIG_SETMASK, &old_set, NULL) == -1)
        exit_msg("Error while setting mask");

    if (is_zero) {
        // block further signals
        sigset_t block_set;
        sigaddset(&block_set, SIGRTMIN);
        if (sigprocmask(SIG_BLOCK, &block_set, NULL) == -1)
            exit_msg("Error while blocking signal");
    }

    // wait for all children to terminate
    while (wait(NULL) > 0)
        continue;

    // clang-format off
    if (me == root) {
#ifdef DEBUG
        printf(RED "\nCheck logs directory for per process logs" RESET "\n");
#endif
        printf(RED "\nAll children are done. Process %d (root node) will exit" RESET "\n", me);
    } else {
        char msg[200];
        if(is_zero) {
            snprintf(msg, sizeof(msg), "Process %d will exit. Reason: <= 0 Points. Total signals caught: %d", me, cnt_signals);
            fflush(stdout);
            printf(GRN "%s" RESET "\n", msg);
        }
        else {
            snprintf(msg, sizeof(msg), "Process %d will exit. Reason: Timed out waiting for signals. Total signals caught: %d", me, cnt_signals);
            fflush(stdout);
            printf(YEL "%s" RESET "\n", msg);
        }
#ifdef DEBUG
        char msg2[202];
        snprintf(msg2, sizeof(msg2), "%s\n", msg);
        log_entry(me, msg);
#endif
    }
    // clang-format on

    return EXIT_SUCCESS;
}
