
include common.mk

APP = client
CLIENT_SRC := client.cpp
SERVER_SRC := server.cpp
CFLAGS += -Wall -std=c++11 -O3

all: clean build/client build/server

build/client: $(CLIENT_SRC) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build/server: $(SERVER_SRC) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build:
	@mkdir -p $@

clean:
	rm -f build/*
	test -d build && rmdir -p build || true
