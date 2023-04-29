# TODO: Make this Makefile.in pretty
include ../common/common.mk

MTCP_COMMON_INC = /mnt/Work/Networks/Project/mTCP
BUILDDIR=./build

INC += -I${MTCP_COMMON_INC}

all : clean $(BUILDDIR) $(BUILDDIR)/server $(BUILDDIR)/client

$(BUILDDIR) :
	mkdir $(BUILDDIR)

$(BUILDDIR)/server : $(BUILDDIR)/server.o ${MTCP_FLD}/lib/libmtcp.a
	$(MSG) "   LD $<"
	$(HIDE) ${CC} $< ${LIBS} ${UTIL_OBJ} -o $@

$(BUILDDIR)/client : $(BUILDDIR)/client.o ${MTCP_FLD}/lib/libmtcp.a
	$(MSG) "   LD $<"
	$(HIDE) ${CC} $< ${LIBS} ${UTIL_OBJ} -o $@

$(BUILDDIR)/server.o : server.cpp
	$(MSG) "   CC $<"
	$(HIDE) ${CC} -c $< ${CFLAGS} ${INC} -o $@

$(BUILDDIR)/client.o : client.cpp
	$(MSG) "   CC $<"
	$(HIDE) ${CC} -c $< ${CFLAGS} ${INC} -o $@

clean:
	$(MSG) "   CLEAN $(BUILDDIR) log_*"
	$(HIDE) rm -rf *~ $(BUILDDIR) log_*