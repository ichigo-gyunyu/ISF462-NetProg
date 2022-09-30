#include "Broker/broker.h"
#include "Utils/utils.h"
#include "Utils/vector.h"

#define TOPICS_FILE "data/topics.txt"
#define OUT         "publisher"

static Vector *topics;
static int     brokerfd;

static void    usage();
static void    handlerSIGPIPE(int sig);
static void    connBroker(const char *addr);
static void    addTopic();
static void    sendMsg();
static void    sendMsgs();
static Vector *loadTopics(const char *topics_file);
static void    viewTopics(const Vector *topics);
static bool    validateTopic(const char *topic);

int main(int argc, char **argv) {

    if (argc != 2)
        usage();

    connBroker(argv[1]);
    topics = loadTopics(TOPICS_FILE);

    // setup sigpipe handler
    struct sigaction sa;
    sa.sa_handler = handlerSIGPIPE;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        perror_and_exit("failed to setup sigpipe handler");

    int choice = 0;
    for (;;) {
        printf("\n------- PUBLISHER -------\n");
        printf("0. Exit\n");
        printf("1. Add a topic\n");
        printf("2. Send a message\n");
        printf("3. Send series of messages from file\n");
        printf("4. View all topics\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        switch (choice) {

        case 0:
            close(brokerfd);
            exit(EXIT_SUCCESS);

        case 1:
            addTopic();
            break;

        case 2:
            sendMsg();
            break;

        case 3:
            sendMsgs();
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

static void addTopic() {

    flushstdin();

    char tmp[TMP_BUFLEN];
    printf("\nTopic name (max 512 characters): ");
    if (readLine(stdin, tmp, TMP_BUFLEN) == NULL)
        return;

    // update topics vector
    char *topic = strndup(tmp, TMP_BUFLEN);
    vec_pushBack(topics, &topic);

    // update topics.txt
    FILE *fp = fopen(TOPICS_FILE, "a");
    if (fp == NULL)
        perror_and_exit("error opening topics file");
    fprintf(fp, "%s\n", topic);
    fclose(fp);

    printf("Added %s\n", topic);
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

static void sendMsg() {

    flushstdin();

    char tmp[TMP_BUFLEN];
    printf("\nTopic: ");
    if (readLine(stdin, tmp, TMP_BUFLEN) == NULL)
        return;

    if (!validateTopic(tmp)) {
        printf(RED "Invalid topic name" RST "\n");
        return;
    }

    char tmp2[TMP_BUFLEN];
    printf("\nMessage: ");
    if (readLine(stdin, tmp2, TMP_BUFLEN) == NULL)
        return;

    struct msg msg;
    strncpy(msg.topic, tmp, TMP_BUFLEN);
    strncpy(msg.msg, tmp2, TMP_BUFLEN);
    if (write(brokerfd, &msg, sizeof msg) == -1) {
        perror("error sending message");
        return;
    }

    printf("Message sent to broker\n");
}

static void sendMsgs() {

    flushstdin();

    char filename[TMP_BUFLEN];
    printf("\nFilename: ");
    if (readLine(stdin, filename, TMP_BUFLEN) == NULL)
        return;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("could not open file");
        return;
    }

    char topic[TMP_BUFLEN];
    printf("\nTopic: ");
    if (readLine(stdin, topic, TMP_BUFLEN) == NULL)
        return;

    if (!validateTopic(topic)) {
        printf(RED "Invalid topic name" RST "\n");
        return;
    }

    char tmp[TMP_BUFLEN];
    while (readLine(stdin, tmp, TMP_BUFLEN) != NULL) {
        struct msg msg;
        strncpy(msg.topic, topic, TMP_BUFLEN);
        strncpy(msg.msg, tmp, TMP_BUFLEN);
        if (write(brokerfd, &msg, sizeof msg) == -1) {
            perror("error while sending message");
            continue;
        }

        printf("Message sent to broker\n");
    }
}

static void connBroker(const char *addr) {

    // create socket
    if ((brokerfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perror_and_exit("could not create socket");

    // setup address structure
    struct sockaddr_in brokeraddr = {
        .sin_family = AF_INET,
        .sin_port   = htons(BROKER_PUB_PORT),
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
