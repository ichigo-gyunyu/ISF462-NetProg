#include "Broker/broker.h"
#include "Utils/utils.h"
#include "Utils/vector.h"

#define OUT         "subscriber"
#define TOPICS_FILE "data/topics.txt"

static Vector       *topics;
static int           brokerfd;
static char          subscribed[TMP_BUFLEN];
static unsigned long last_seen;

static void    usage();
static void    connBroker(const char *addr);
static void    handlerSIGPIPE(int sig);
static void    subscribe();
static bool    retrieveOne();
static void    retrieveAll();
static Vector *loadTopics(const char *topics_file);
static void    viewTopics(const Vector *topics);
static bool    validateTopic(const char *topic);

int main(int argc, char **argv) {

    if (argc != 2)
        usage();

    subscribed[0] = '\0';

    connBroker(argv[1]);
    topics = loadTopics(TOPICS_FILE);

    // setup sigpipe handler
    struct sigaction sa;
    sa.sa_handler = handlerSIGPIPE;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        perror_and_exit("failed to setup sigpipe handler");

    int choice = 0;
    for (;;) {
        printf("\n------- SUBSCRIBER -------\n");
        printf("0. Exit\n");
        printf("1. Subscribe to a topic\n");
        printf("2. Retrieve a message\n");
        printf("3. Retrieve all messages\n");
        printf("4. View all topics\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        switch (choice) {

        case 0:
            close(brokerfd);
            exit(EXIT_SUCCESS);

        case 1:
            subscribe();
            break;

        case 2:
            if (!retrieveOne())
                printf("\nNo new messages\n");
            break;

        case 3:
            retrieveAll();
            break;

        case 4:
            viewTopics(topics);
            break;

        default:
            printf(RED "\nInvalid choice" RST "\n");
            flushstdin();
            break;
        }
    }
}

static void usage() {
    printf("Usage: " OUT " <broker address>\n");
    exit(EXIT_FAILURE);
}

static void connBroker(const char *addr) {

    // create socket
    if ((brokerfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perror_and_exit("could not create socket");

    // setup address structure
    struct sockaddr_in brokeraddr = {
        .sin_family = AF_INET,
        .sin_port   = htons(BROKER_SUB_PORT),
    };
    inet_pton(AF_INET, addr, &brokeraddr.sin_addr);

    // connect
    if (connect(brokerfd, (struct sockaddr *)&brokeraddr, sizeof brokeraddr) == -1)
        perror_and_exit("Connect error");

    printf("Connected to broker\n");
}

static void handlerSIGPIPE(int sig) {
    printf(RED "Received SIGPIPE while trying to write\n");
    printf(RED "Broker no longer active. Exiting...\n");

    exit(EXIT_FAILURE);
}

static void subscribe() {

    flushstdin();

    printf("\nTopic name (max 512 characters): ");
    if (readLine(stdin, subscribed, TMP_BUFLEN) == NULL)
        return;

    if (!validateTopic(subscribed)) {
        subscribed[0] = '\0';
        printf(RED "Invalid topic name" RST "\n");
        return;
    }

    last_seen = 0;
    printf("Subscribed to %s\n", subscribed);
}

static bool retrieveOne() {

    // tell the broker which topic we are subscribed to
    if (write(brokerfd, &subscribed, TMP_BUFLEN) == -1) {
        perror("error retrieving message");
        return false;
    }

    // tell the broker the timestamp of the last seen message
    if (write(brokerfd, &last_seen, sizeof last_seen) == -1) {
        perror("error retrieving message");
        return false;
    }

    struct msg msg;
    ssize_t    n = read(brokerfd, &msg, sizeof msg);
    if (n < 0) {
        perror("error retrieving message");
        return false;
    }

    if (msg.topic[0] == '\0')
        return false;

    last_seen = strtoul(msg.topic, NULL, 10);

    printf("\n");
    printf("Message ID: %s\n", msg.topic);
    printf("Message: %s\n", msg.msg);
    printf("\n");
    return true;
}

static void retrieveAll() {

    while (retrieveOne())
        ;
    printf("No more messages\n");
}

static Vector *loadTopics(const char *topics_file) {

    FILE *fp = fopen(topics_file, "r");
    if (fp == NULL)
        perror_and_exit("error opening topics file");

    Vector *vec = vec_init_ptr();
    char    tmp[TMP_BUFLEN];
    while (readLine(fp, tmp, TMP_BUFLEN) != NULL) {
        char *topic = strndup(tmp, TMP_BUFLEN);
        vec_pushBack(vec, &topic);
    }

    fclose(fp);

    return vec;
}

static void viewTopics(const Vector *topics) {

    if (topics == NULL) {
        printf("No topics have been added\n");
        return;
    }

    printf("\n");

    for (int i = 0; i < topics->size; i++) {
        char *topic = vec_getValAt(topics, i);
        printf("%s\n", topic);
    }

    printf("\n");
}

static bool validateTopic(const char *topic) {
    if (topics == NULL)
        return false;

    for (int i = 0; i < topics->size; i++) {
        if (strcmp(topic, vec_getValAt(topics, i)) == 0)
            return true;
    }

    return false;
}
