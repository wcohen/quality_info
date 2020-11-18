GCCXXFLAGS=-fvar-tracking-assignments -gstatement-frontiers -gvariable-location-views
CXXFLAGS=-O2 -g -std=c++17
LDFLAGS=-L /usr/lib64/dyninst \
	-l parseAPI -l symtabAPI -l instructionAPI \
	-l tbb -l common

TOOLS=liveness locrangechecks cse readonly
all: $(TOOLS)

clean:
	rm -rf *.o *~ *.ii *.s *.o
	rm -rf $(TOOLS)
