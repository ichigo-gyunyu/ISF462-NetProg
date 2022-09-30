MAKEFLAGS += --silent

CC = gcc
CFLAGS = -Wall
# CFLAGS = -Wall -Wextra -g -fsanitize=address -fsanitize=leak
LDFLAGS = -lrt
OBJS = shell.o parser.o process.o job.o utils.o
OUT = shell

all: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(OUT) $(LDFLAGS)
	echo "Usage: ./$(OUT) or ./$(OUT) -v for detailed info"

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUT) $(OBJS)
