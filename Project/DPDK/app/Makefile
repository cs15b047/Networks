TOPDIR=/mnt/Work/f-stack

ifeq ($(FF_PATH),)
	FF_PATH=${TOPDIR}
endif

ifneq ($(shell pkg-config --exists libdpdk && echo 0),0)
$(error "No installation of DPDK found, maybe you should export environment variable `PKG_CONFIG_PATH`")
endif

PKGCONF ?= pkg-config

CFLAGS += -O3 -gdwarf-2 $(shell $(PKGCONF) --cflags libdpdk)

LIBS+= $(shell $(PKGCONF) --static --libs libdpdk)
LIBS+= -L${FF_PATH}/lib -Wl,--whole-archive,-lfstack,--no-whole-archive
LIBS+= -Wl,--no-whole-archive -lrt -lm -ldl -lcrypto -pthread -lnuma

TARGET=fstack
BUILDDIR=./build

all: clean $(BUILDDIR)
	cc ${CFLAGS} -o $(BUILDDIR)/${TARGET}_server server.c ${LIBS}
	cc ${CFLAGS} -o $(BUILDDIR)/${TARGET}_server_epoll server_epoll.c ${LIBS}
	cc ${CFLAGS} -o $(BUILDDIR)/${TARGET}_client client.c ${LIBS}

$(BUILDDIR):
	mkdir $(BUILDDIR)
.PHONY: clean
clean:
	rm -f *.o $(BUILDDIR)/*
