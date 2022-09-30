#include "utils.h"

void perror_and_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

char *readLine(FILE *fp, char *buf, const int n) {

    if (fgets(buf, n, fp) == NULL)
        return NULL;

    // strip trailing newline, if any
    if (buf[strlen(buf) - 1] == '\n')
        buf[strlen(buf) - 1] = '\0';

    return buf;
}

void flushstdin() {

    char c;
    while ((c = getchar()) != '\n')
        continue;
}
