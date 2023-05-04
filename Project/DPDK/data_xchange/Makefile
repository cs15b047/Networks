
include common.mk

SORT_SRC := app.cpp
CFLAGS += -std=c++11 -O3

all: clean build/app

build/app: $(SORT_SRC) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

debug: CFLAGS += -g
debug: all

build:
	@mkdir -p $@

clean:
	rm -f build/*
	test -d build && rmdir -p build || true
