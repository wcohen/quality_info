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

## Gray Areas

Depending on the specifics there could be cases where live patching can
be performed on these.  Gray areas ares where we need more
information collected from actual programs to determine how much of an
issue these will be.


### Cold sections of code

The GCC compiler in some cases will split out rarely executed code to
improve the locality of frequently executed code.  For a function
named *foo* the code code will be placed in an area labeled with
*foo*.cold.  The compiler should still generate range information in
rnglist function for the original function foo including code in the
.cold section.

Having the cold sections of code for a function is unlikely in itself
to cause significant problems.  However, some analysis tools may not
deal well with jumps in and out of the function proper or may miss
analyzing the code in the .cold suffixed regions due to multiple entry
points.

### Constant Propagation

The GCC compiler may do interprocedural analysis and determine that
some function calls alway pass constants for some arguments for a call
to another function.  The compiler may create versions of the function
simplify things considerably do to the constants eliminating some dead
code.  For example on the vmlinux-5.12.7-300.fc34 kernel has the
complicated preallocate_pmds function below:

```
static int preallocate_pmds(struct mm_struct *mm, pmd_t *pmds[], int count)
{
	int i;
	bool failed = false;
	gfp_t gfp = GFP_PGTABLE_USER;

	if (mm == &init_mm)
		gfp &= ~__GFP_ACCOUNT;

	for (i = 0; i < count; i++) {
		pmd_t *pmd = (pmd_t *)__get_free_page(gfp);
		if (!pmd)
			failed = true;
		if (pmd && !pgtable_pmd_page_ctor(virt_to_page(pmd))) {
			free_page((unsigned long)pmd);
			pmd = NULL;
			failed = true;
		}
		if (pmd)
			mm_inc_nr_pmds(mm);
		pmds[i] = pmd;
	}

	if (failed) {
		free_pmds(mm, pmds, count);
		return -ENOMEM;
	}

	return 0;
}
```

The above preallocate_pmds above gets converted into a much simpler
code below that is essentially the "return 0" statement at the end of
the function:

```
ffffffff81076820 <preallocate_pmds.constprop.0>:
ffffffff81076820:	e8 bb ea fe ff       	callq  ffffffff810652e0 <__fentry__>
ffffffff81076825:	31 c0                	xor    %eax,%eax
ffffffff81076827:	c3                   	retq   
ffffffff81076828:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
ffffffff8107682f:	00 
```

One of the common approaches of the CVE bandaids using systemtap is to
trigger the code to force the code to take some path through the code.
Maybe to turn off an problematic option or force an error code, but
for the above preallocate_pmds.constprop.0 it always returns 0.  The
code in the for loop can never be executed.

Below it does appear when pgd_alloc calls prellocate_pmds.constprop.0
it is setting up the first two arguments, but the third argument,
count, is 0.  With count of 0 the function simplifies to "return 0":


```
ffffffff81076bc6:	31 d2                	xor    %edx,%edx
ffffffff81076bc8:	48 8d 74 24 08       	lea    0x8(%rsp),%rsi
ffffffff81076bcd:	4c 89 e7             	mov    %r12,%rdi
ffffffff81076bd0:	48 89 c5             	mov    %rax,%rbp
ffffffff81076bd3:	49 89 c5             	mov    %rax,%r13
ffffffff81076bd6:	e8 45 fc ff ff       	callq  ffffffff81076820 <preallocate_pmds.constprop.0>
ffffffff81076bdb:	85 c0                	test   %eax,%eax
ffffffff81076bdd:	0f 85 ae 01 00 00    	jne    ffffffff81076d91 <pgd_alloc+0x201>
ffffffff81076be3:	31 d2                	xor    %edx,%edx
ffffffff81076be5:	48 89 e6             	mov    %rsp,%rsi
ffffffff81076be8:	4c 89 e7             	mov    %r12,%rdi
ffffffff81076beb:	e8 30 fc ff ff       	callq  ffffffff81076820 <preallocate_pmds.constprop.0>
ffffffff81076bf0:	85 c0                	test   %eax,%eax
ffffffff81076bf2:	0f 85 99 01 00 00    	jne    ffffffff81076d91 <pgd_alloc+0x201>
```

Another example is __recount_add:

```
static inline void __refcount_add(int i, refcount_t *r, int *oldp)
{
	int old = atomic_fetch_add_relaxed(i, &r->refs);

	if (oldp)
		*oldp = old;

	if (unlikely(!old))
		refcount_warn_saturate(r, REFCOUNT_ADD_UAF);
	else if (unlikely(old < 0 || old + i < 0))
		refcount_warn_saturate(r, REFCOUNT_ADD_OVF);
}
```

The argument i has been set to 1 and oldp is NULL, only one argument passed in via %rdi:

```
ffffffff8124d080 <__refcount_add.constprop.0>:
ffffffff8124d080:	b8 01 00 00 00       	mov    $0x1,%eax
ffffffff8124d085:	f0 0f c1 07          	lock xadd %eax,(%rdi)
ffffffff8124d089:	85 c0                	test   %eax,%eax
ffffffff8124d08b:	74 12                	je     ffffffff8124d09f <__refcount_add.constprop.0+0x1f>
ffffffff8124d08d:	8d 50 01             	lea    0x1(%rax),%edx
ffffffff8124d090:	09 c2                	or     %eax,%edx
ffffffff8124d092:	78 01                	js     ffffffff8124d095 <__refcount_add.constprop.0+0x15>
ffffffff8124d094:	c3                   	retq   
ffffffff8124d095:	be 01 00 00 00       	mov    $0x1,%esi
ffffffff8124d09a:	e9 f1 53 3c 00       	jmpq   ffffffff81612490 <refcount_warn_saturate>
ffffffff8124d09f:	be 02 00 00 00       	mov    $0x2,%esi
ffffffff8124d0a4:	e9 e7 53 3c 00       	jmpq   ffffffff81612490 <refcount_warn_saturate>
ffffffff8124d0a9:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
```

### Interprocedural Scalar Replacement of Aggregates (ISRA)

Interprocedural Scalar Replacement of Aggregates (ISRA) [^isra]
optimization breaks apart aggregate structures into individual scalars
where possible.  This allows the compiler to remove unused portions of
the aggregate structure and perform finer-grain optimizations on the
individual scalars extracted the aggregate.  This optimization changes
the ABI of the generated function.  To distinguish the function that
has undergone ISRA optimization and would have a different ABI the
optimized function has a .isra suffix on the function name.  Below is
an source code the trace_event_name function in the Fedora 34 Linux
2.12.7-300.x86_64 kernel which the GCC compiler produces a ISRA
version.

[^isra]: https://gcc.gnu.org/wiki/summit2010?action=AttachFile&do=get&target=jambor.pdf



```
static inline const char *
trace_event_name(struct trace_event_call *call)
{
	if (call->flags & TRACE_EVENT_FL_TRACEPOINT)
		return call->tp ? call->tp->name : NULL;
	else
		return call->name;
}
```

The original function above has one struct being passed in call (a
pointer to struct trace_event_call), but in the generated code below
there are two scalar arguments being passed in: %rdi (unamed union that
has elements char *name and struct tracepoint *tp) and %esi (flags
from a struct trace_event_call).  Thus, it would not be possible
examine any other fields of the struct.

```
ffffffff81b9387b <trace_event_name.isra.0>:
ffffffff81b9387b:	40 80 e6 10          	and    $0x10,%sil
ffffffff81b9387f:	48 89 f8             	mov    %rdi,%rax
ffffffff81b93882:	74 08                	je     ffffffff81b9388c <trace_event_name.isra.0+0x11>
ffffffff81b93884:	48 85 ff             	test   %rdi,%rdi
ffffffff81b93887:	74 03                	je     ffffffff81b9388c <trace_event_name.isra.0+0x11>
ffffffff81b93889:	48 8b 07             	mov    (%rdi),%rax
ffffffff81b9388c:	c3                   	retq   
```

Also systemtap (systemtap-4.5-1.fc34.x86_64) appears to be confused
and still lists the original argument even though it doesn't exist:

```
$ stap -r 5.12.7-300.fc34.x86_64  -v -L 'kernel.function("trace_event_name*").call'
Pass 1: parsed user script and 791 library scripts using 3479656virt/3245244res/12508shr/3232600data kb, in 7230usr/710sys/7995real ms.
kernel.function("trace_event_name@./include/linux/trace_events.h:391").call /* pc=_stext+0xb9387b */ $call:struct trace_event_call*
Pass 2: analyzed script: 1 probe, 0 functions, 0 embeds, 0 globals using 3731268virt/3498004res/13748shr/3484212data kb, in 1650usr/250sys/2464real ms.
```

The trace_event_name.isra.0 is only called from .cold sections.  There
are a number of places where the function has also been inlined which
could have scalar replacement of aggregates:

```
$ stap -r 5.12.7-300.fc34.x86_64  -v -L 'kernel.function("trace_event_name*").inline'
Pass 1: parsed user script and 791 library scripts using 3479656virt/3244940res/12208shr/3232600data kb, in 7230usr/670sys/7926real ms.
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1dad56 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1e8e14 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1eaaaa */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1e8d67 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1e91a9 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1e9601 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1e8674 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1e8d19 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1e991a */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1ef403 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0xb93afa */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1f8c08 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1f9469 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1f96c8 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1fa717 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1fa936 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1f75f9 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1f5d10 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x2015bd */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1ff2c7 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x2013a6 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1ff0f2 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x200daf */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1fef37 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1fefb5 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0xb93bf1 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x1ff25e */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x209da8 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x2085ec */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x208718 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x207dda */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x208cd2 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x207d67 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x208466 */ $call:struct trace_event_call*
kernel.function("trace_event_name@./include/linux/trace_events.h:391").inline /* pc=_stext+0x207ebc */ $call:struct trace_event_call*
Pass 2: analyzed script: 35 probes, 0 functions, 0 embeds, 0 globals using 3732096virt/3498392res/13344shr/3485040data kb, in 1660usr/120sys/1805real ms.
```


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



