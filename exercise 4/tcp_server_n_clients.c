/*
 * tcp_server_n_clients.c
 *
 * Name: Lingesh Kumaar
 * ID: 2018B4A70857P
 *
 * Usage:
 * make
 * ./tcp_server_n_clients n
 *
 * Concurrent TCP server that handles atmost n clients through
 * a fork() per client approach.
 * Parent-Child communication is done using System V message queues
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

#define BUFF        4096
#define LISTENQ     10
#define PORT        12344
#define OUTFILE     "tcp_server_n_clients"
#define NUM_ELEM(x) (sizeof(x) / sizeof((x)[0]))

// data structure for the message queue
struct msgbuf {
    long mtype;
    char mtext[1];
};

static unsigned int num_conns = 0;
static int          msqid;
static pid_t        parent_pid;

// helper function
void usage() {
    printf("Usage ./" OUTFILE " n\n");
    printf("n - max clients\n");
    exit(EXIT_FAILURE);
}

// helper function
void perr_exit(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Handle SIGCHLD to avoid zombies
static void reaper(int signo) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        num_conns--; // since SIGCHLD is ignored inside the handler, we can safely decrement

        // check the message queue if it was a normal termination
        struct msgbuf tmpbuf;
        if (msgrcv(msqid, &tmpbuf, 1, 0, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG)
                printf("Client terminated abnormally. Currently active number of clients: %u\n", num_conns);
        } else
            printf("Client terminated. Currently active number of clients: %u\n", num_conns);
    }
}

// Delete message queues before exiting
static void term_handler(int sig) {
    if (getpid() != parent_pid)
        exit(EXIT_SUCCESS);

    if (msgctl(msqid, IPC_RMID, NULL) == -1)
        perr_exit("Failed to delete message queue");

    exit(EXIT_SUCCESS);
}

/**
 * As the main purpose of this server is to
 * be able to limit the number of clients,
 * the server's responsibility is to just read
 * any message from client and reply back with OK
 */
void handle_client(int connfd) {
    ssize_t n;
    char    buf[BUFF];
    char    ok[]       = "OK";
    char    accepted[] = "Connection accepted";

    // send acceptance message
    write(connfd, accepted, strlen(accepted));

    for (;;) {
        n = read(connfd, buf, BUFF);

        if (n == -1) {
            if (errno == EINTR)
                continue; // try reading again
            else
                perr_exit("Read error");
        }

        else if (n == 0) {
            break; // FIN due to EOF
        }

        write(connfd, ok, 3);
    }
}

// reject connection when n active clients already exist
void reject_client(int connfd) {
    printf("Max number of clients reached. Refusing connection\n");
    close(connfd);
}

int main(int argc, char **argv) {
    if (argc != 2)
        usage();

    parent_pid = getpid();

    unsigned int n = (int)strtol(argv[1], NULL, 10);

    // setup the socket
    int listenfd, connfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perr_exit("Could not create socket");

    // setup the address structure
    struct sockaddr_in cliaddr, servaddr;
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(PORT);

    // bind and listen
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof servaddr) == -1)
        perr_exit("Bind error");
    if (listen(listenfd, LISTENQ) == -1)
        perr_exit("Listen error");

    // handler for SIGCHLD
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = reaper;
    sigaction(SIGCHLD, &sa, NULL);

    // handler for termination
    struct sigaction sa2;
    sa2.sa_handler   = term_handler;
    const int sigs[] = {SIGINT, SIGTERM};
    uint      m      = NUM_ELEM(sigs);
    for (uint i = 0; i < m; i++) {
        if (sigaction(sigs[i], &sa2, NULL) == -1)
            perr_exit("Failed to setup interrupt handler");
    }

    // message queue for parent-child communication
    msqid = msgget(IPC_PRIVATE, S_IRUSR | S_IWUSR);
    if (msqid == -1)
        perr_exit("Failed to create message queue");

    // server loop
    for (;;) {
        socklen_t clilen = sizeof cliaddr;
        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) == -1) {
            if (errno == EINTR)
                continue; // since accept is not restarted
            else
                perr_exit("Listen error");
        }

        if (num_conns == n) {
            reject_client(connfd);
            continue;
        }
        num_conns++;
        printf("Connection accepted. Currently active clients: %u\n", num_conns);

        // handle client in a child process
        pid_t childpid;
        if ((childpid = fork()) == 0) {
            // child process
            close(listenfd);
            handle_client(connfd);
            close(connfd);

            // inform parent about successful termination of child
            msgsnd(msqid, &(struct msgbuf){.mtype = 1}, 1, 0);

            exit(EXIT_SUCCESS);
        }

        close(connfd);
    }
}
