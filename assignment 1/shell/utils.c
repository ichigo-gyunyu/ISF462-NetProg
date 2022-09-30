#include "utils.h"

void perror_and_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
