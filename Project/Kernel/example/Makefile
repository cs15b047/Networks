CC=gcc

all: clean client server

client: 
	$(CC) -O3 -o client client.c

server: 
	$(CC) -O3 -o server server.c
	$(CC) -O3 -o server_epoll server_epoll.c


clean:
	rm -f client server server_epoll
