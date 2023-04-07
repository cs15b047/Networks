CC=gcc
LFLAGS= -lrdmacm

client: client.c
	$(CC) -o client client.c $(LFLAGS)

server: server.c
	$(CC) -o server server.c $(LFLAGS)

all: client server

clean:
	rm -f client server