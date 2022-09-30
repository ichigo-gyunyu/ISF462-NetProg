#include "broker.h"

#define LISTENQ            10
#define MESSAGE_TIME_LIMIT 60

static pid_t parent_pid;
static int   pubfd;
static int   subfd;
static int   connfd;
static int   gotalarm;
static char *msg_dir;

static void setupPublisher();
static void setupSubscriber();
static void handlePublisher(const int connfd);
static void handleSubscriber(const int connfd);
static void cleanOldMsg();

static void term_handler(int sig) {

    // remove msg_dir, (use system calls instead of rm -rf)
    char rem_call[TMP_BUFLEN];
    snprintf(rem_call, TMP_BUFLEN, "rm -rf %s", msg_dir);
    system(rem_call);

    exit(EXIT_SUCCESS);
}

static void term_handler_publisher(int sig) {
    close(pubfd);
    term_handler(sig);
}

static void term_handler_subscriber(int sig) {
    close(subfd);
    term_handler(sig);
}

static void alarm_handler(int sig) {
    gotalarm = 1;
    alarm(MESSAGE_TIME_LIMIT);
}

int main() {

    parent_pid = getpid();
    gotalarm   = 0;

    // setup the directory for storing messages
    char template[] = "/tmp/msgdir.XXXXXX";
    if ((msg_dir = mkdtemp(template)) == NULL)
        perror_and_exit("could not create tmp directory");

    // separate out broker-publisher and broker-subscriber
    switch (fork()) {
    case -1:
        perror_and_exit("fork failed");
    case 0:
        setupPublisher();
        break;
    default:
        setupSubscriber();
        break;
    }
}

static void setupPublisher() {

    // setup SIGALRM handler
    struct sigaction sa;
    sa.sa_handler = alarm_handler;
    sa.sa_flags   = 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1)
        perror_and_exit("failed to setup interrupt handler");

    // handler for termination
    struct sigaction sa2;
    sa2.sa_handler   = term_handler_publisher;
    const int sigs[] = {SIGINT, SIGTERM};
    uint      m      = NUM_ELEM(sigs);
    for (uint i = 0; i < m; i++) {
        if (sigaction(sigs[i], &sa2, NULL) == -1)
            perror_and_exit("failed to setup interrupt handler");
    }

    // setup socket
    if ((pubfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perror_and_exit("could not create socket");

    // setup address structure
    struct sockaddr_in servaddr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htons(INADDR_ANY),
        .sin_port        = htons(BROKER_PUB_PORT),
    };

    // bind and listen
    if (bind(pubfd, (struct sockaddr *)&servaddr, sizeof servaddr) == -1)
        perror_and_exit("bind error");
    if (listen(pubfd, LISTENQ) == -1)
        perror_and_exit("listen error");

    // start the cleanup alarm
    alarm(MESSAGE_TIME_LIMIT);

    // server loop
    for (;;) {

        if (gotalarm)
            cleanOldMsg();

        struct sockaddr_in cliaddr;
        socklen_t          clilen = sizeof cliaddr;
        if ((connfd = accept(pubfd, (struct sockaddr *)&cliaddr, &clilen)) == -1) {
            if (errno == EINTR)
                continue;
            else
                perror_and_exit("listen error");
        }

        printf("Connected to Publisher\n");

        // handle publisher in a new process
        switch (fork()) {
        case -1:
            perror("fork error");
            break;
        case 0:
            close(subfd);
            handlePublisher(connfd);
            close(connfd);
            exit(EXIT_SUCCESS);
        default:
            break;
        }

        close(connfd);
    }
}

static void setupSubscriber() {

    // handler for termination
    struct sigaction sa;
    sa.sa_handler    = term_handler_subscriber;
    const int sigs[] = {SIGINT, SIGTERM};
    uint      m      = NUM_ELEM(sigs);
    for (uint i = 0; i < m; i++) {
        if (sigaction(sigs[i], &sa, NULL) == -1)
            perror_and_exit("failed to setup interrupt handler");
    }

    // setup socket
    if ((subfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perror_and_exit("could not create socket");

    // setup address structure
    struct sockaddr_in servaddr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htons(INADDR_ANY),
        .sin_port        = htons(BROKER_SUB_PORT),
    };

    // bind and listen
    if (bind(subfd, (struct sockaddr *)&servaddr, sizeof servaddr) == -1)
        perror_and_exit("bind error");
    if (listen(subfd, LISTENQ) == -1)
        perror_and_exit("listen error");

    // server loop
    for (;;) {
        struct sockaddr_in cliaddr;
        socklen_t          clilen = sizeof cliaddr;
        if ((connfd = accept(subfd, (struct sockaddr *)&cliaddr, &clilen)) == -1) {
            if (errno == EINTR)
                continue;
            else
                perror_and_exit("listen error");
        }

        printf("Connected to Subscriber\n");

        // handle subscriber in a new process
        switch (fork()) {
        case -1:
            perror("fork error");
            break;
        case 0:
            close(subfd);
            handleSubscriber(connfd);
            close(connfd);
            exit(EXIT_SUCCESS);
        default:
            break;
        }

        close(connfd);
    }
}

static void handlePublisher(const int connfd) {

    ssize_t    n;
    struct msg msg;

    DIR *dp = opendir(msg_dir);
    if (dp == NULL)
        perror_and_exit("could not open message directory");
    int dfd = dirfd(dp);
    if (dfd == -1)
        perror_and_exit("directory fd error");

    for (;;) {
        n = read(connfd, &msg, sizeof msg);

        if (n == -1) {
            if (errno == EINTR)
                continue;
            else
                perror_and_exit("read error");
        }

        else if (n == 0) {
            continue;
        }

        char topic_dir[TMP_BUFLEN];
        snprintf(topic_dir, TMP_BUFLEN, "%s/%s", msg_dir, msg.topic);

        // open the topic directory
        DIR *td = opendir(topic_dir);
        if (td == NULL) {
            if (errno != ENOENT)
                perror_and_exit("could not open directory");
            if (mkdirat(dfd, msg.topic, S_IRWXU) == -1)
                perror_and_exit("could not create directory");
        }

        // prepare file name
        char epochtime[TMP_BUFLEN];
        char filename[TMP_BUFLEN];
        snprintf(epochtime, TMP_BUFLEN, "%lu", (unsigned long)time(NULL));
        snprintf(filename, TMP_BUFLEN, "%s/%s", topic_dir, epochtime);

        // save msg to file
        FILE *fp = fopen(filename, "a");
        if (fp == NULL)
            perror_and_exit("could not open file for writing");
        fprintf(fp, "%s", msg.msg);
        fclose(fp);

        printf("Received message from publisher. Topic: %s\n", msg.topic);
    }

    closedir(dp);
}

static void handleSubscriber(const int connfd) {

    ssize_t       n;
    char          topic[TMP_BUFLEN];
    unsigned long last_seen;

    for (;;) {
        // read topic
        if ((n = read(connfd, &topic, sizeof topic)) == -1)
            perror_and_exit("read error");

        // read last seen
        if ((n = read(connfd, &last_seen, sizeof last_seen)) == -1)
            perror_and_exit("read error");

        // open topic directory
        char dirname[TMP_BUFLEN];
        snprintf(dirname, TMP_BUFLEN, "%s/%s", msg_dir, topic);
        DIR *dp = opendir(dirname);
        if (dp == NULL)
            perror_and_exit("could not open topic directory");

        // check for new message
        struct msg msg;
        msg.topic[0]          = '\0';
        bool           newmsg = false;
        struct dirent *ent;
        while ((ent = readdir(dp)) != NULL) {
            if (ent->d_type == DT_REG) {
                unsigned long timestamp = strtoul(ent->d_name, NULL, 10);
                if (timestamp <= last_seen)
                    continue;

                newmsg = true;
                strncpy(msg.topic, ent->d_name, TMP_BUFLEN);
                char filename[TMP_BUFLEN];
                snprintf(filename, TMP_BUFLEN, "%s/%s", dirname, ent->d_name);

                // read message from file
                FILE *fp = fopen(filename, "r");
                if (fp == NULL)
                    perror_and_exit("could not open file");
                if (readLine(fp, msg.msg, TMP_BUFLEN) == NULL)
                    perror_and_exit("could not read from file");

                fclose(fp);
            }
        }

        // send message
        if (write(connfd, &msg, sizeof msg) == -1)
            perror_and_exit("error while sending message");

        if (newmsg)
            printf("Sent message to subscriber. Topic: %s\n", topic);

        closedir(dp);
    }
}

static void cleanOldMsg() {

    gotalarm = 0;

    DIR *dp = opendir(msg_dir);
    if (dp == NULL)
        perror_and_exit("could not open message directory");

    // recursively traverse the message directory
    unsigned long  curtime = (unsigned long)time(NULL);
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            char path[TMP_BUFLEN];
            snprintf(path, TMP_BUFLEN, "%s/%s", msg_dir, ent->d_name);

            // open subdirectory
            DIR *dpt = opendir(path);
            if (dpt == NULL) {
                perror("could not open topic directory");
                continue;
            }

            // delete all old messages in subdirectory
            struct dirent *entt;
            while ((entt = readdir(dpt)) != NULL) {
                if (entt->d_type == DT_REG) {
                    unsigned long timestamp = strtoul(entt->d_name, NULL, 10);
                    if (curtime - timestamp > MESSAGE_TIME_LIMIT) {
                        char filename[TMP_BUFLEN];
                        snprintf(filename, TMP_BUFLEN, "%s/%s", path, entt->d_name);
                        if (remove(filename) == -1)
                            perror_and_exit("could not delete file");
                        printf("removed old message %s\n", filename);
                    }
                }
            }
        }
    }

    closedir(dp);
}
