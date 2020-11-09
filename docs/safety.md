# Spectrum of Safey for Live Patching




## Relatively Safe

### Function Entry and Return

The ABI for regular function calls and returns are well defined for
various architectures.  The locations of parameters passed into
function on entry and return values of the function are easy to find
and modify.

There are restrictions on patching function entries and exits.
Functions that are used by the live patching infrastructure (kprobes)
cannot be patched as that would cause a processor to get stuck in
recursion when handling a live patch.  We need to be careful about
functions with optimized tail calls.  Might not observe the expected
multiple entries a recursive function calling itself.  I have also
observed cases of wrapper functions that the call at the end has been
turned into a jmp to the function being wrapped, so the return
instruction triggering the return may be in another function.

#### Experiments examining function entry return locations

Would like to know is how much inlining is going on and how much that
is going to affect traditional functions.  The first experiment is to
see what portion of easily instrumented function entry/returns are
eliminated.  The second experiment is to determine whether there are
cases wither there are multiple versions of the same same function.

1. Compare systemtap `*.function("*").call` `*.function("*").inline` between variants
1. Compare systemtap `*.function("*").call` `*.function("*").inline` in same version

## Gray area

Depending on the specific there could be cases where live patching can
be performed on these.  The gray area is where we need more
information collected from actual programs to determine how much of an
issue these will be.

### Inlined Functions

For performance compilers are going attempt to inline functions to
eliminate function prologues and epilogues.  Also the compiler may be
better able to eliminate unneed code by being able to analyze the code
across function boundaries and schedule instructions between functions.

With inlined function there is no a well defined return or return
value.  Live patching is not going to work on those missing function
returns.

#### Experiments examining function entry return locations

1. Compare systemtap `*.function("*").inline` arguments between inlined placed

### Specific line numbers

There may be cases where the entry/return of a function is not where
the live patch needs to occur and a specific line in the code is where
the patch to be placed.  This is can be fragile and line numbers can
change significantly between builds due to patches.  However, the
buildid numbering might be able restrict the patching to specific
executables.

This is going to need some more thought on how to analyze the
generated code and the possible success in installing live patches in
those locations.  The idea that a variable is in a particular location
is less specificed than function ABI.  There may be multiple values
for a variable in different locations at the same time.  The debug
infomation may point to locations that do have the correct value, but
changes to that value will have no effect.  The compiler may also
perform common subexpression elimination, so a single location may be
actually affect expressions outside the one being targetted.


## Hazardous

Currently, the kernel developers need to manually mark the functions
that are hazardous so they are added a black list to indicate they are
not suitable for probing.  This annotation is done with the
NOKPROBE_SYMBOL macro.  The kprobe logic generates a set of address
ranges from that information and check it to make sure that a probe
point does not fall in any of the ranges on that list.

### Regions implementing atomic operations

Processors such as ARM and powerpc have load-linked/store-exclusive
instructions to implement atomic operations.  These instruction
provide a method of detecting if the atomic operation has been
interrupted and that the action should be retried.  If a kprobe is
inserted in those atomic regions, the software breakpoint used to
implement the kprobe will always show that the attempt at the atomic
operation has been interrupted.

Transactional memory operations supported by processors could also
have similar issues as the load-linked/store-exclusive instructions.

Are there ways to check for these instructions?
And identify these regions?

Many atomic primitives are implemented as inlined functioned to reduce
the overhead of the operation.  How to ensure that do not end up
putting a live patch in the interior or edge of one of these areas?
This might be complicated as there might be multiple views for an
instruction.  An instruction could be line x of function a, but it
could also be line y of inlined atomic primitive function b.

How does that interact when inlined functions are marked in with
kprobes black lists?  Does it mark regions of the text section off
limits?  Or is it just the coarser, marking traditional functions?

### Functions used by the kprobes

The machinery used to implement the probing uses some functions in the
kernel that are also used by other functions.  Using kprobes on those
would cause kprobe handling to trigger another kprobe handler before
finishing the initial one and the processor would never make progress.
Thus, instrumenting functions that kprobes might use is forbidden
because it is unsafe.



