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

struct addrinfo *host_serv(const char *host, const char *serv, int family, int socktype) {
    int             n;
    struct addrinfo hints, *res;

    hints = (struct addrinfo){
        .ai_flags    = AI_CANONNAME,
        .ai_family   = family,
        .ai_socktype = socktype,
    };

    if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
        perror_and_exit("getaddrinfo");

    return res;
}

char *sock_ntop_host(const struct sockaddr *sa, socklen_t salen) {
    static char str[128]; /* Unix domain is largest */

    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in *sin = (struct sockaddr_in *)sa;

        if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
            perror_and_exit("inet ntop ipv4");
        return str;
    }

    case AF_INET6: {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

        if (inet_ntop(AF_INET6, &sin6->sin6_addr, str, sizeof(str)) == NULL)
            perror_and_exit("inet ntop ipv6");
        return str;
    }

    case AF_UNIX: {
        struct sockaddr_un *unp = (struct sockaddr_un *)sa;

        /* OK to have no pathname bound to the socket: happens on
           every connect() unless client calls bind() first. */
        if (unp->sun_path[0] == 0)
            strcpy(str, "(no pathname bound)");
        else
            snprintf(str, sizeof(str), "%s", unp->sun_path);
        return str;
    }
    }

    return NULL;
}

void sock_set_port(struct sockaddr *sa, int port) {
    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in *sin = (struct sockaddr_in *)sa;

        sin->sin_port = port;
        return;
    }

    case AF_INET6: {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

        sin6->sin6_port = port;
        return;
    }
    }

    return;
}
