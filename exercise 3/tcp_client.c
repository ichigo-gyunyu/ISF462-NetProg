/*
 * tcp_client.c
 *
 * Name: Lingesh Kumaar
 * ID: 2018B4A70857P
 *
 * Usage:
 * make
 * ./tcp_client addr
 *
 * Example: ./tcp_client www.google.com
 *
 * Establish a TCP connection and send a simple GET request
 * to a web server 100 times. Displays all responses/errors and
 * measures average time taken for receiving a response
 *
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT       "80"
#define BUFFERSIZE 2048
#define NUMROUNDS  100

void usage() {
    printf("Usage: ./tcp_client addr\n");
    printf("\naddr is an IP address or URL\n");
    printf("Eg: ./tcp_client www.google.com\n");
    exit(EXIT_FAILURE);
}

void exit_msg(char *msg) {
    fprintf(stdout, "%s\n", msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc != 2)
        usage();

    int             sockfd;
    struct addrinfo hints, *results;

    // prepare for getaddrinfo
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_flags    = AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(argv[1], PORT, &hints, &results) != 0)
        exit_msg("getaddrinfo() error");

    /**
     * Perform a connection request and a simple HTTP get request
     * and display the response.
     * Measure the time taken for each request and compute the average.
     */
    char *send_buf = calloc(BUFFERSIZE, sizeof *send_buf);
    char *recv_buf = calloc(BUFFERSIZE, sizeof *recv_buf);
    // HTTP GET request
    snprintf(send_buf, BUFFERSIZE,
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Accept: text/html\r\n"
             "\r\n",
             argv[1]);

    // time measuring utils
    struct timespec tic, toc;
    double          elapsed;
    double          total = 0;

    for (uint i = 0; i < NUMROUNDS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &tic);

        // attempt connection using one of the addrinfo structures
        struct addrinfo *t;
        for (t = results; t != NULL; t = t->ai_next) {

            if ((sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol)) == -1)
                continue;

            if (connect(sockfd, t->ai_addr, t->ai_addrlen) == 0)
                break; // success
        }
        if (t == NULL)
            exit_msg("Could not connect");

        printf("\nConnection established\n");

        // send
        if (send(sockfd, send_buf, BUFFERSIZE, 0) == -1) {
            perror("Send Failed");
            continue;
        }

        // receive
        ssize_t recv_bytes;
        if ((recv_bytes = recv(sockfd, recv_buf, BUFFERSIZE, 0)) <= 0) {
            printf("Receive Failed\n");
        } else {
            printf("----- RESPONSE %03d -----\n", i);
            recv_buf[recv_bytes - 1] = '\0'; // truncate response
            printf("%s\n\n", recv_buf);
        }

        clock_gettime(CLOCK_MONOTONIC, &toc);
        elapsed = toc.tv_sec - tic.tv_sec;
        elapsed += (toc.tv_nsec - tic.tv_nsec) / 1000000000.0;
        elapsed *= 1000;
        printf("Elapsed time: %f ms\n", elapsed);
        total += elapsed;
    }

    double av = total / NUMROUNDS;
    printf("\nSTATS\n");
    printf("Total time:   %.6f ms\n", total);
    printf("Average time: %.6f ms\n", av);

    free(send_buf);
    free(recv_buf);
    freeaddrinfo(results);
}
