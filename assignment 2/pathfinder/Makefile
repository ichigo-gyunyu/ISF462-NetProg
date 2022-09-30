MAKEFLAGS += --silent

CC = gcc
CFLAGS = -Wall -pthread
# CFLAGS = -Wall -g -fsanitize=address -pthread
LDFLAGS = -lanl
INC =
OUT = pathfinder
OBJS = pathfinder.o \
	   utils.o \
	   vector.o \
	   hashtable.o

all: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(OUT) $(LDFLAGS)

pathfinder.o: pathfinder.c $(wildcard Utils/*)
	$(CC) $(CFLAGS) $(INC) -c pathfinder.c

utils.o: $(wildcard Utils/*)
	$(CC) $(CFLAGS) $(INC) -c Utils/utils.c

vector.o: $(wildcard Utils/*)
	$(CC) $(CFLAGS) $(INC) -c Utils/vector.c

hashtable.o: $(wildcard Utils/*)
	$(CC) $(CFLAGS) $(INC) -c Utils/hashtable.c

clean:
	rm -rf $(OUT) $(OBJS)
