# compile examples showing the compiler generating code
# that is hard to inspect

CFLAGS=-g -O3

EXECUTABLES = variables_same_name

all: $(EXECUTABLES)

variables_same_name: variables_same_name.c
	$(CC) -save-temps $(CFLAGS) $< -o $@
	objdump -d $@ >  $@.dis

.PHONY : all default clean

clean:
	rm -f *.o *.dis *.i *.s  $(EXECUTABLES)
