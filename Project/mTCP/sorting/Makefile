CFALGS = -Wall -Wextra -Werror -pedantic -std=c++11
include ../common/common.mk

MTCP_COMMON_INC = /mnt/Work/Networks/Project/mTCP
BUILDDIR=./build

INC += -I${MTCP_COMMON_INC}

all : clean $(BUILDDIR) $(BUILDDIR)/sort

$(BUILDDIR) :
	mkdir $(BUILDDIR)

$(BUILDDIR)/sort : $(BUILDDIR)/sort.o ${MTCP_FLD}/lib/libmtcp.a
	$(MSG) "   LD $<"
	$(HIDE) ${CC} $< ${LIBS} ${UTIL_OBJ} -o $@

$(BUILDDIR)/sort.o : sort.cpp
	$(MSG) "   CC $<"
	$(HIDE) ${CC} -c $< ${CFLAGS} ${INC} -o $@

clean :
	rm -rf $(BUILDDIR)