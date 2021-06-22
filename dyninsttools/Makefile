GCCXXFLAGS=-fvar-tracking-assignments -gstatement-frontiers -gvariable-location-views
CXXFLAGS=-O2 -g -std=c++17
LDFLAGS=-L /usr/lib64/dyninst \
	-l parseAPI -l symtabAPI -l instructionAPI \
	-l tbb -l common

TOOLS=liveness locrangechecks cse readonly empty_loclist ranges_sanity
all: $(TOOLS)

liveness: liveness.C common.a

locrangechecks: locrangechecks.C common.a

cse: cse.C common.a

readonly: readonly.C common.a

common.o: common.C common.h

common.a: common.o
	$(AR) ru $@ $^

clean:
	rm -rf *.o *~ *.ii *.s *.o
	rm -rf $(TOOLS) common.a
