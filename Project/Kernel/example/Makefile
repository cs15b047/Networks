CC=gcc

all: clean client server

CFLAGS=-O3
CDEBUGFLAGS=-g

client: 
	$(CC) $(CDEBUGFLAGS) -o client client.c

server: 
	$(CC) $(CDEBUGFLAGS) -o server server.c
	$(CC) $(CDEBUGFLAGS) -o server_epoll server_epoll.c


clean:
	rm -f client server server_epoll
