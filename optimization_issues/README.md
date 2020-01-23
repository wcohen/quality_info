This directory holds files that attempt to trigger the generation of code that might
have some debuginfo that might present issues:

variables_same_name.c  both main and fact have variable i alive at the same time.


To get an idea of the variables available in the executable can do
something like the following.  The example below shows at
pc=.absolute+0x10a3 both the main and fact functions have variable $i
alive.  However, the line number does not look correct for the inline
fact function.


$ stap -v -L 'process("./variables_same_name").statement("*@*:*")' | sed "s,$(pwd),,g"|sort -t\+ -k2
Pass 1: parsed user script and 575 library scripts using 1253760virt/1024344res/13156shr/1011076data kb, in 2250usr/320sys/2574real ms.
Pass 2: analyzed script: 11 probes, 0 functions, 0 embeds, 0 globals using 1310600virt/1082528res/14252shr/1067916data kb, in 310usr/10sys/320real ms.
process("/variables_same_name").statement("main@/variables_same_name.c:16") /* pc=.absolute+0x1060 */ $argc:int $argv:char** $i:int
process("/variables_same_name").statement("main@/variables_same_name.c:19") /* pc=.absolute+0x1079 */ $argc:int $argv:char** $i:int
process("/variables_same_name").statement("main@/variables_same_name.c:20") /* pc=.absolute+0x1089 */ $argc:int $argv:char** $i:int
process("/variables_same_name").statement("main@/variables_same_name.c:8") /* pc=.absolute+0x108d */ $argc:int $argv:char** $i:int
process("/variables_same_name").statement("fact@/variables_same_name.c:8") /* pc=.absolute+0x108d */ $result:int $i:int
process("/variables_same_name").statement("main@/variables_same_name.c:9") /* pc=.absolute+0x10a0 */ $argc:int $argv:char** $i:int
process("/variables_same_name").statement("fact@/variables_same_name.c:9") /* pc=.absolute+0x10a0 */ $result:int $i:int
process("/variables_same_name").statement("main@/variables_same_name.c:10") /* pc=.absolute+0x10a3 */ $argc:int $argv:char** $i:int
process("/variables_same_name").statement("fact@/variables_same_name.c:10") /* pc=.absolute+0x10a3 */ $result:int $i:int
process("/variables_same_name").statement("main@/variables_same_name.c:21") /* pc=.absolute+0x10a8 */ $argc:int $argv:char** $i:int $j:int
process("/variables_same_name").statement("main@/variables_same_name.c:22") /* pc=.absolute+0x10b4 */ $argc:int $argv:char** $i:int
