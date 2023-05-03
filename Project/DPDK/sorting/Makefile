
include common.mk

SORT_SRC := sort.cpp
CFLAGS +=  -std=c++11 -O3
LDFLAGS += -lpthread

all: clean build/sort

build/sort: $(SORT_SRC) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build:
	@mkdir -p $@

clean:
	rm -f build/*
	test -d build && rmdir -p build || true
