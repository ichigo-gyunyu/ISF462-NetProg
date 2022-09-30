MAKEFLAGS += --silent

CC = gcc
CFLAGS = -Wall
DEBUGFLAGS = -DDEBUG
LOGDIR = logs
OBJS = $(SRCS:.c=.o)
OUT = signal_tree
SRCS = $(wildcard *.c)

build: compile
	$(CC) $(OBJS) -o $(OUT)
	echo "Done! Usage ./$(OUT) N A S"

compile:
	$(CC) $(CFLAGS) -c $(SRCS)

debug:
	rm -rf $(LOGDIR)
	mkdir $(LOGDIR)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c $(SRCS)
	$(CC) $(OBJS) -o $(OUT)

test: build
	./$(OUT) 5 3 3

clean:
	rm -rf $(OBJS) $(OUT) $(LOGDIR)
