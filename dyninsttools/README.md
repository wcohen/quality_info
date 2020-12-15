# Dyninst tooling to inspect binaries

Tools such as dwarfdump will analyze the structure of the debuginfo,
but they do not examine the binaries to see if the debuginfo agrees
with the binary.  Dyninst is a tool that allows inspection and
analysis of executables.  This directory holds programs written to
look for issue in the debuginfo generated.

- cse.C - Determine which variables share the same register
- liveness.C - experimental code to better understand dyninst's liveness analysis
- locrangechecks.C -  Do some simple verification of the variable location ranges
- readonly.C - Determine which variable register writes have no effect.
