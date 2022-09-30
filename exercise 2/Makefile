MAKEFLAGS += --silent

CC = gcc
CFLAGS = -Wall
OBJS = $(SRCS:.c=.o)
OUT = voting
SRCS = $(wildcard *.c)

build: compile
	$(CC) $(OBJS) -o $(OUT)
	echo "Done! Usage ./$(OUT) n"

compile:
	$(CC) $(CFLAGS) -c $(SRCS)

clean:
	rm -rf $(OBJS) $(OUT)
