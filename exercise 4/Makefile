MAKEFLAGS += --silent

CC = gcc
# CFLAGS = -Wall
CFLAGS = -Wall -g -fsanitize=address
OUT = tcp_server_n_clients

all: tcp_server_n_clients.o tcp_client.o
	$(CC) $(CFLAGS) tcp_server_n_clients.o -o $(OUT)
	$(CC) $(CFLAGS) tcp_client.o -o tcp_client
	echo "Done! Usage ./tcp_server_n_clients n"
	echo "To test with clients: ./tcp_client"

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm -rf *.o $(OUT) tcp_client
