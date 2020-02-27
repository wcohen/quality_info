GCCXXFLAGS=-fvar-tracking-assignments -gstatement-frontiers -gvariable-location-views
CXXFLAGS=-g3 -std=c++17
LDFLAGS=-L /usr/lib64/dyninst \
	-l parseAPI -l symtabAPI -l instructionAPI \
	-l tbb -l common

TOOLS=liveness
all: $(TOOLS)

clean:
	rm -rf *.o *~
	rm -rf $(TOOLS)
