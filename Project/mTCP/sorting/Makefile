CFALGS = -Wall -Wextra -Werror -pedantic -std=c++11
BUILDDIR = build

all : clean $(BUILDDIR) $(BUILDDIR)/sort

$(BUILDDIR) :
	mkdir $(BUILDDIR)

$(BUILDDIR)/sort: sort.cpp
	g++ $(CFALGS) sort.cpp  -o $@

clean :
	rm -rf $(BUILDDIR)