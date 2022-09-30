MAKEFLAGS += --silent

CC = gcc
CFLAGS = -Wall
# CFLAGS = -Wall -g -fsanitize=address
OUT = tcp_client

all: tcp_client.o
	$(CC) $(CFLAGS) $^ -o $(OUT)
	echo "Usage: ./$(OUT) addr"
	echo "addr is an IP address or URL"
	echo "Eg: ./tcp_client www.google.com"


%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm -rf *.o $(OUT)
