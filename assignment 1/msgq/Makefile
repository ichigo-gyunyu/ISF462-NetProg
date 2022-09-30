MAKEFLAGS += --silent

CC = gcc
CFLAGS = -Wall -g -Wno-format-truncation
# CFLAGS = -Wall -g -fsanitize=address
LDFLAGS =
INC = -I./src
OUT_PUB = publisher
OUT_BRO = broker
OUT_SUB = subscriber
OBJS = utils.o \
	   vector.o

all: $(OUT_PUB) $(OUT_BRO) $(OUT_SUB)

$(OUT_PUB): $(OBJS) $(OUT_PUB).o
	$(CC) $(CFLAGS) $(OBJS) $(OUT_PUB).o -o $(OUT_PUB) $(LDFLAGS)

$(OUT_BRO): $(OBJS) $(OUT_BRO).o
	$(CC) $(CFLAGS) $(OBJS) $(OUT_BRO).o -o $(OUT_BRO) $(LDFLAGS)

$(OUT_SUB): $(OBJS) $(OUT_SUB).o
	$(CC) $(CFLAGS) $(OBJS) $(OUT_SUB).o -o $(OUT_SUB) $(LDFLAGS)

publisher.o: $(wildcard src/Publisher/*)
	$(CC) $(CFLAGS) $(INC) -c src/Publisher/publisher.c

broker.o: $(wildcard src/Broker/*)
	$(CC) $(CFLAGS) $(INC) -c src/Broker/broker.c

subscriber.o: $(wildcard src/Subscriber/*)
	$(CC) $(CFLAGS) $(INC) -c src/Subscriber/subscriber.c

utils.o: $(wildcard src/Utils/utils*)
	$(CC) $(CFLAGS) $(INC) -c src/Utils/utils.c

vector.o: $(wildcard src/Utils/vector*)
	$(CC) $(CFLAGS) $(INC) -c src/Utils/vector.c

clean:
	rm -rf $(OUT_PUB) $(OUT_BRO) $(OUT_SUB) $(OBJS) $(OUT_PUB).o $(OUT_BRO).o $(OUT_SUB).o
