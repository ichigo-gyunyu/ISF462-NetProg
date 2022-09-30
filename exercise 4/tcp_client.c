/*
 * tcp_client.c
 *
 * Name: Lingesh Kumaar
 * ID: 2018B4A70857P
 *
 * Usage:
 * make
 * ./tcp_client
 *
 * Simple TCP client to test the n-client server
 *
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT  12344
#define BUFF  4096
#define ADDRS "127.0.0.1"

void perr_exit(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    // setup the socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perr_exit("Socket error");

    // setup the address structure
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(PORT);
    inet_pton(AF_INET, ADDRS, &servaddr.sin_addr);

    printf("Attempting connection...\n");
    // attempt connection
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof servaddr) == -1)
        perr_exit("Connect error");

    // client loop
    char bufsend[BUFF], bufrecv[BUFF];
    for (;;) {
        if (read(sockfd, bufrecv, BUFF) == 0) {
            printf("Server is not accepting requests. Closing...\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        printf("Server Replied: %s\n", bufrecv);

        printf("\nAny message for server (Ctrl+D to close): ");
        if (fgets(bufsend, BUFF, stdin) == NULL)
            break; // EOF
        write(sockfd, "test", strlen("test"));
    }

    close(sockfd);
}
