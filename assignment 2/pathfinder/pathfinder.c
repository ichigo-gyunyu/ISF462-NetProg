#include "Utils/hashtable.h"
#include "Utils/utils.h"
#include "Utils/vector.h"

#define OUT             "pathfinder"
#define MAX_URLS        8192
#define MAX_URL_LEN     128
#define MAX_ADDR_LEN    32
#define START_DEST_PORT 33434
#define MAX_HOPS        30
#define PROBES_PER_HOP  5
#define FAMILY          AF_INET

// stores all relevant information for a URL
typedef struct URLinfo {
    char               *url;         // url from the text file
    char               *host_addr;   // address of url acc to getaddrinfo()
    struct sockaddr_in *sa_send;     // from getaddrinfo() (is updated before each sendto())
    struct sockaddr_in *sa_bind;     // from getaddrinfo()
    socklen_t           salen;       // from getaddrinfo()
    int                 sendfd;      // udp socket that sends datagrams
    uint16_t            src_port;    // to uniquely identify a host
    uint16_t            dest_port;   // updated with each sequence number
    bool                destination; // if after sending all probes, destination was reached

    char path[MAX_HOPS][PROBES_PER_HOP][MAX_ADDR_LEN]; // stores the path to reach destination
} URLinfo;

static char       urls[MAX_URLS][MAX_URL_LEN];
static int        recvfd;
static pid_t      pid;      // for icmp packet identifier
static uint16_t   id;       // unique identifier that builds source ports
static Vector    *urlinfos; // Vector<URLinfo *>
static uint32_t   num_urls;
static Hashtable *srcport_to_url;

void  usage();
void  resolveURLs();
void  addURL(const char *url);
void  updateURLinfo(const char *url, const struct addrinfo *ai);
void  print_urlinfos(); // for debugging
void  recvICMP();
void  sendUDP(const int ttl, const uint32_t to_send, const uint8_t seq);
void *runner(void *arg); // thread function
void  longestPath();

int main(int argc, char **argv) {

    if (argc != 2)
        usage();

    urlinfos       = vec_init_ptr();
    srcport_to_url = ht_init_uint16_void();

    // read and resolve address of URLs from file
    FILE *fp = fopen(argv[1], "r");
    char  tmp[TMP_BUFLEN];
    if (fp == NULL)
        perror_and_exit("could not open input file");
    while (readLine(fp, tmp, sizeof tmp) != NULL)
        addURL(tmp);
    fclose(fp);

    pid = getpid();
    id  = ((pid & 0x2fff) | 0x8000) - num_urls;

    resolveURLs();

    // create the raw socket for receiving icmp data
    if ((recvfd = socket(FAMILY, SOCK_RAW, IPPROTO_ICMP)) == -1)
        perror_and_exit("raw socket creation");
    setuid(getuid()); // no longer need root

    // prepare the threads (one per TTL)
    pthread_t tid[MAX_HOPS];
    for (int i = 1; i <= MAX_HOPS; i++) {
        int *arg = malloc(sizeof *arg);
        *arg     = i;
        pthread_create(&tid[i - 1], NULL, runner, arg);
    }

    // wait on all threads
    for (int i = 1; i <= MAX_HOPS; i++) {
        pthread_join(tid[i - 1], NULL);
    }

    print_urlinfos();

    longestPath();
}

void usage() {
    printf("Usage: ./" OUT "<inputfile>\n");
    exit(EXIT_FAILURE);
}

void addURL(const char *url) {
    strncpy(urls[num_urls], url, TMP_BUFLEN);
    num_urls++;
}

// perform parallel DNS resolutions using getaddrinfo_a
void resolveURLs() {

    struct addrinfo hints = {
        .ai_flags    = AI_CANONNAME,
        .ai_family   = FAMILY,
        .ai_socktype = 0,
    };

    struct gaicb *reqs[num_urls];
    for (int i = 0; i < num_urls; i++) {
        reqs[i]             = calloc(1, sizeof(*reqs[0]));
        reqs[i]->ar_name    = urls[i];
        reqs[i]->ar_request = &hints;
    }

    // parallel DNS lookups
    int ret = getaddrinfo_a(GAI_WAIT, reqs, num_urls, NULL);
    if (ret != 0)
        perror_and_exit(gai_strerror(ret));

    // parse the addrinfo structures received
    for (int i = 0; i < num_urls; i++) {
        if ((ret = gai_error(reqs[i])) != 0)
            perror_and_exit(gai_strerror(ret));
        updateURLinfo(reqs[i]->ar_name, reqs[i]->ar_result);
    }
}

void updateURLinfo(const char *url, const struct addrinfo *ai) {

    URLinfo *urlinfo = malloc(sizeof *urlinfo);

    // work with only ipv4 for now
    // create a separate bind and send socket, that only share same family
    urlinfo->url                 = strdup(url);
    urlinfo->sa_send             = (struct sockaddr_in *)ai->ai_addr;
    urlinfo->salen               = ai->ai_addrlen;
    urlinfo->sa_bind             = calloc(1, urlinfo->salen);
    urlinfo->host_addr           = strdup(sock_ntop_host((struct sockaddr *)urlinfo->sa_send, urlinfo->salen));
    urlinfo->sa_bind->sin_family = urlinfo->sa_send->sin_family;
    urlinfo->dest_port           = START_DEST_PORT;
    urlinfo->src_port            = id + urlinfos->size;
    urlinfo->sa_bind->sin_port   = htons(urlinfo->src_port);
    urlinfo->sendfd              = socket(urlinfo->sa_send->sin_family, SOCK_DGRAM, 0);
    urlinfo->destination         = false;

    // set path initially to * (timeout indicator)
    for (int i = 0; i < MAX_HOPS; i++) {
        for (int j = 0; j < PROBES_PER_HOP; j++) {
            strncpy(urlinfo->path[i][j], "*", sizeof(urlinfo->path[0][0]));
        }
    }

    if (urlinfo->sendfd == -1)
        perror_and_exit("dgram socket creation");

    // bind
    if (bind(urlinfo->sendfd, urlinfo->sa_bind, urlinfo->salen) == -1)
        perror_and_exit("bind error");

    // add to urlinfos vector
    vec_pushBack(urlinfos, &urlinfo);

    // add the srcport to url mapping into the hashtable
    ht_insert(&srcport_to_url, &(urlinfo->src_port), &urlinfo);
}

void *runner(void *arg) {

    int ttl = *(int *)arg;

    // setup the epoll instance that checks if raw socket is readable
    int epollfd = epoll_create(1);
    if (epollfd == -1)
        perror_and_exit("epoll creation");

    struct epoll_event ev = {.events = EPOLLIN, .data.fd = recvfd};
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, recvfd, &ev) == -1)
        perror_and_exit("epoll ctl add");

    uint8_t  seq     = 0;
    uint32_t to_send = 0;
    for (;;) {
        if (seq == PROBES_PER_HOP)
            break; // done sending for this TTL value

        // non blocking check
        ev.events = 0;
        epoll_wait(epollfd, &ev, 1, 0);

        // ready to read
        if (ev.events & EPOLLIN) {
            recvICMP();
        } else {
            sendUDP(ttl, to_send, seq);
            to_send++;
            if (to_send == num_urls) {
                to_send = 0;
                seq++;
            }
        }
    }

    // wait for any stray replies
    ev.events = 0;
    while (epoll_wait(epollfd, &ev, 1, 5000) != 0) {
        if (ev.events & EPOLLIN)
            recvICMP();
    }

    close(epollfd);
    free(arg);

    return NULL;
}

void sendUDP(const int ttl, const uint32_t to_send, const uint8_t seq) {

    // get the corresponding URLinfo
    URLinfo *uinfo = vec_getValAt(urlinfos, to_send);

    // set the ttl value
    if (setsockopt(uinfo->sendfd, IPPROTO_IP, IP_TTL, &ttl, sizeof ttl) == -1)
        perror_and_exit("setsockopt");

    // encode the ttl and seq number into the destination socket
    uinfo->sa_send->sin_port = htons(uinfo->dest_port + (ttl * PROBES_PER_HOP + seq));

    // send a 1 byte UDP datagram to this high (unsued) port number
    if (sendto(uinfo->sendfd, "", 1, 0, uinfo->sa_send, uinfo->salen) == -1)
        perror_and_exit("sendto error");
}

void recvICMP() {

    char            recvbuf[TMP_BUFLEN];
    struct sockaddr sa_recv;
    socklen_t       len;

    ssize_t n = recvfrom(recvfd, recvbuf, sizeof recvbuf, 0, &sa_recv, &len);
    if (n == -1)
        perror_and_exit("recvfrom error");

    // parse ip header
    struct ip *ip      = (struct ip *)recvbuf;
    int        iph_len = ip->ip_hl << 2;

    // parse icmp header
    struct icmp *icmp     = (struct icmp *)(recvbuf + iph_len);
    int          icmp_len = n - iph_len;
    if (icmp_len < 8)
        return; // icmp header is 8 bytes

    if (icmp_len < 8 + sizeof(struct ip))
        return; // ip header of the original udp datagram sent

    // parse the ip header that was sent
    struct ip *sent_ip      = (struct ip *)(recvbuf + iph_len + 8); // skip icmp header
    int        sent_iph_len = sent_ip->ip_hl << 2;
    if (icmp_len < 8 + sent_iph_len + 4)
        return; // udp ports are 2 bytes each

    // parse the udp header that was sent
    struct udphdr *udp = (struct udphdr *)(recvbuf + iph_len + 8 + sent_iph_len);

    // check the sent ip header's protocol
    if (sent_ip->ip_p != IPPROTO_UDP)
        return;

    // use the sent udp header's source port to determine the destination URL
    uint16_t src_port = ntohs(udp->uh_sport);
    URLinfo *uinfo    = ht_lookupVal(srcport_to_url, &src_port);
    if (uinfo == NULL)
        return; // some other ICMP packet

    // use the sent udp header's destination port to determine ttl and seq
    uint16_t ttlseq = ntohs(udp->uh_dport) - START_DEST_PORT;
    uint8_t  ttl    = ttlseq / PROBES_PER_HOP;
    uint8_t  seq    = ttlseq % PROBES_PER_HOP;
    if (ttl < 0 || ttl > MAX_HOPS || seq < 0 || seq >= PROBES_PER_HOP)
        return; // some other ICMP packet

    // ttl 0 before destination (at intermediate router)
    if (icmp->icmp_type == ICMP_TIME_EXCEEDED && icmp->icmp_code == ICMP_TIMXCEED_INTRANS) {

        // update path
        char *addr = sock_ntop_host(&sa_recv, len);
        if (addr != NULL)
            strncpy(uinfo->path[ttl - 1][seq], addr, sizeof(uinfo->path[0][0]));

    }

    // at destination or some other error
    else if (icmp->icmp_type == ICMP_UNREACH) {
        if (icmp->icmp_code == ICMP_UNREACH_PORT || icmp->icmp_code == ICMP_UNREACH_FILTER_PROHIB) {
            uinfo->destination = true; // destination
            strncpy(uinfo->path[ttl - 1][seq], "dest", sizeof(uinfo->path[0][0]));
        }

        else {
            strncpy(uinfo->path[ttl - 1][seq], "x", sizeof(uinfo->path[0][0])); // some other error
        }
    }
}

void print_urlinfos() {

    for (int i = 0; i < urlinfos->size; i++) {
        URLinfo *uinfo = vec_getValAt(urlinfos, i);
        printf("URL: %s Address: %s\n", uinfo->url, uinfo->host_addr);
        printf("        %16s %16s %16s %16s %16s\n", "Probe 1", "Probe 2", "Probe 3", "Probe 4", "Probe 5");

        for (int j = 0; j < MAX_HOPS; j++) {
            printf("TTL %2d: ", j + 1);
            for (int k = 0; k < PROBES_PER_HOP; k++) {
                printf("%16s ", uinfo->path[j][k]);
            }
            printf("\n");
        }

        printf("\n\n");
    }
}

void longestPath() {

    char     start[PROBES_PER_HOP][MAX_ADDR_LEN];
    URLinfo *uinfo = vec_getValAt(urlinfos, 0);

    for (int i = 0; i < MAX_HOPS; i++) {
        for (int j = 0; j < PROBES_PER_HOP; j++) {
            strncpy(start[j], uinfo->path[i][j], MAX_ADDR_LEN);
        }

        bool allmatch = true;
        for (int j = 1; j < urlinfos->size; j++) {
            bool     match = false;
            URLinfo *ui    = vec_getValAt(urlinfos, j);
            for (int k = 0; k < PROBES_PER_HOP; k++) {
                for (int l = 0; l < PROBES_PER_HOP; l++) {
                    if (strncmp(start[k], ui->path[i][l], MAX_ADDR_LEN) == 0) {
                        match = true;
                        break;
                    }
                }
                if (match)
                    break;
            }
            if (!match) {
                allmatch = false;
                break;
            }
        }

        if (!allmatch) {
            printf("Longest Common Path Length: %d\n", i - 1);
            break;
        }

        bool printed = false;
        for (int j = 0; j < PROBES_PER_HOP; j++) {
            if (strncmp("*", start[j], MAX_ADDR_LEN) != 0) {
                printf("TTL %2d: %s\n", i, start[j]);
                printed = true;
            }
        }
        if (!printed)
            printf("TTL %2d: *\n", i);
    }
}
