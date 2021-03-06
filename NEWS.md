# RISCV-VM News

----
### 18 Oct. 2020

The last few days have been spent inserting an intermediate representation step into the JIT codepath.  There is now an instruction decoder which takes in an instruction word and spits out a decoded instruction as a struct which can be passed onto the codegen step.  The intention here was that I could decode a basic block entirely prior to the codegen phase thus giving the codegen more visibility about the block that is being translated.  The most interesting metric to me is knowing if a block is a leaf function or not.

An unexpected side effect was that it really improved the code quality, with a very clear partition between the JIT phases.

I also instrumented the decoder to keep track of how many times it encounters instructions while decoding a basic block.  Below is a list of the relative odds of encountering an instruction when decoding a basic block.  This should not be confused with how many times and instruction will be executed, but its closely related.  Knowing this gives me an interesting insight into which instructions I should be looking to tighten up the generated code for.

Instruction decode frequencies when booting Doom:
```
23.6%   addi          15.6%   lw            12.3%   sw            6.6%    auipc
 5.7%   jal            4.7%   add            4.4%   slli          3.4%    beq
 2.5%   lbu            2.1%   or             2.1%   bne           1.8%    xor
 1.7%   jalr           1.7%   srli           1.4%   sub           1.3%    sb
 1.6%   blt            1.0%   bge             .9%   andi           .7%    srai
  .6%   lui             .5%   and             .4%   lh             .4%    sh
  .3%   bgeu            .3%   bltu            .2%   mul            .1%    lhu
  .1%   div            ...    xori           ...    sll           ...     divu
 ...    sltu           ...    sltiu          ...    remu          ...     slt
 ...    sra            ...    lb             ...    ori           ...     ecall
 ...    rem            ...    srl            ...    mulh          0       slti
 0      fence          0      ebreak         0      mulhsu        0       mulhu
```

----
### 15 Oct. 2020

I am still looking at improving some of the worst parts of the generated JIT code.  Keeping track of the hit-count per translated basic block turned out to be really useful as I can make sure i'm analyzing the hottest basic blocks which should have the most impact.  This morning I turned my attention to the code generated by the branch instructions:

```
// before
24: 8b 86 9c 00 00 00       mov    eax,DWORD PTR [rsi+0x9c]     ; load rs1
2a: 8b 56 70                mov    edx,DWORD PTR [rsi+0x70]     ; load rs2
2d: 39 d0                   cmp    eax,edx                      ; compare
2f: b8 28 ac 01 00          mov    eax,0x1ac28                  ; PC + 4
34: ba cc ab 01 00          mov    edx,0x1abcc                  ; PC + imm
39: 0f 44 c2                cmove  eax,edx                      ; select target
3c: 89 86 d0 00 00 00       mov    DWORD PTR [rsi+0xd0],eax     ; rv->PC = eax
```

While I may rethink how I handle this instruction, there is a quick improvement that can be made:

```
// before
24: 8b 86 9c 00 00 00       mov    eax,DWORD PTR [rsi+0x9c]     ; load rs1
2a: 8b 56 70                cmp    eax,DWORD PTR [rsi+0x70]     ; compare with rs2
2f: b8 28 ac 01 00          mov    eax,0x1ac28                  ; PC + 4
34: ba cc ab 01 00          mov    edx,0x1abcc                  ; PC + imm
39: 0f 44 c2                cmove  eax,edx                      ; select target
3c: 89 86 d0 00 00 00       mov    DWORD PTR [rsi+0xd0],eax     ; rv->PC = eax
```

Unfortunately `cmov` is fairly limited with regard to the operands that it can accept which makes the remaining code a little lacking.  It still gets `Quake` hitting the 450 MIPS mark at some points which is cool.  `Doom` still shows no change making me think there is a bottleneck elsewhere that I should be hunting down.

A different strategy to handle branches would be as follows (in pseudo assembly):
```
...
mov eax, rv->X[rs1]
cmp eax, rv->X[rs2]
je taken
mov rv->PC, not_taken
; insert prologue
ret
.taken
; continue translation from taken PC
...
```

Like this we could essentially fall out of a block if the branch is not taken, thus optimizing for the taken case.  It is just an idea however and I doubt i'll be implementing it any time soon though.

----
### 15 Oct. 2020

At this point I have achieved much more then I had in mind when I set out the original goals for the project.  I did not expect to get Doom and Quake running or let alone to have JIT working.  At this stage I have to decide which direction I want future work to focus on as I feel aimless development would be mostly wasted effort.  There are two possible directions that the project could move;  increasing the complexity of the simulation to the point where it can launch linux for instance, and the other to focus on becoming a high performance user mode simulator.  The second direction has the most narrow scope with a focus on speed, accuracy and compatibility.  The former is more technically difficult, requiring MMU emulation, multiple hart support, etc.

While putting off this decision, I have been taking stock of the codebase, cleaning things up and having a look at the generated code.  I tried multiple different ways of producing intermediate code before the JIT but was ultimately unhappy with both of them.  The first method generated something akin to SSA code for each basic block but my design was too inflexible to be practical.  My second attempt was more flexible and generated something like expression trees for various instructions, but again it ended up not feeling like the right approach.  A third attempt is forming in my mind but it likely a ways off being committed to.

One area that has improved a lot was that the JIT core has now become sufficiently fully featured that it doesn't need the emulation core to fallback on since they have almost the same level of support for instructions.  Due to this, I was able to split the two cores entirely into separate modules with the ability to swap cores easily in CMake.

Support for traps has also been added to the emulation core which can now pass the misaligned compliance tests.

I have done a small amount of performance tuning on the JIT code, dumping the generated basic blocks, disassembling them and looking for simple ways to make the code better.

The RISC-V LUI instruction was fairly poor and has been improved:
```
// before
mov    eax,0x668b8
mov    DWORD PTR [rsi+0xd0],eax

// after
mov    DWORD PTR [rsi+0xd0], 0x668b8
```

Lots of the branch and PC related code was also be updated to not use an intermediate register too which had a positive impact on the execution times.

For store instructions the value to be stored needs to be loaded into r8 before the store call, which could be improved:
```
// before
mov    eax, DWORD PTR [rsi+0x70]
mov    r8,rax

// after
movsx  r8, DWORD PTR [rsi+0x70]
```

Another thing I have considered is that leaf functions don't require a stack frame, which could simplify the prologue and epilogue:
```
// before
push   rbp
mov    rbp,rsp
sub    rsp,0x40
mov    QWORD PTR [rsp+0x20],rsi
...
mov    rsi,QWORD PTR [rsp+0x20]
mov    rsp,rbp
pop    rbp
ret

// after (for leaf functions)
push rsi
; align?
...
; pop align?
pop rsi
ret
```

To know if you are generating a leaf function however would require an IR representation, deferred prologue/epilogue generation or a lookahead decoder since you need to know if you will generate any call instructions. This optimization will have to wait for a future update however.

Another improvement to make is to simply leave the `rv` structure in the RCX register and not move it into the RSI register.  It would simplify the prologue/epilogue and simplify any call operations. I picked RSI mostly on a whim when I started writing the JIT and realise it was a poor choice now.

I also tried to add in dynamic next block prediction but successfully slowed down the core once again which was interesting.  It is still a bit of a puzzle to me why tracking and updating the last successor block taken would be worse than keeping that prediction static.  My profiling runs show that about 10% of execution time is spent searching for the next block to execute, with a near 100% hash map hit rate (no further probing required).  The bulk of the time is likely spent computing the hash of the address which is used to index the hash map.

Getting back to looking at the code, there are frequent fragments similar to the code below:
```
// after 1
99: 8b 86 88 00 00 00       mov    eax,DWORD PTR [rsi+0x88]
9f: c1 e0 02                shl    eax,0x2
a2: 89 86 88 00 00 00       mov    DWORD PTR [rsi+0x88],eax

// after 2
1a: 8b 86 88 00 00 00       mov    eax,DWORD PTR [rsi+0x88]
20: 05 88 f8 ff ff          add    eax,0xfffff888
25: 89 86 88 00 00 00       mov    DWORD PTR [rsi+0x88],eax
```

These could be replaced with much shorter in-place operations:
```
// after 1
shl dword ptr [rsi+0x88], 0x2

// after 2
and dword ptr [rsi+0x88], 0xfffff888
```

I suspect these fragments are generated by immediate operations when `rd` matches `rs1`.  It may be possible to apply similar optimizations to the register-register ALU operations too.

A further optimization I made was to treat constant jumps with no linking as not a branch and continue translation of the code stream.  This seemed like the blocks would get larger and there would be less calls to `block_find`.  It is perplexing however that this made no difference in terms of MIPS at all.

After making these optimizations `Quake` is now running at around 440 MIPS which is a large improvement over where it started.  Quite strange however is that `Doom` still sits at the 550 MIPS mark being relatively unaffected by these improvements.

----
### 04 Oct. 2020

Some time was spent today looking at the largest bottlenecks in the generated code which were in RISC-V register file access and callbacks when doing io and syscalls.  Both of these operations would involve accessing the members of a structure which stores the state of the emulation.  The DynRec would place the address of these members into a register and then perform a write or read from that address.  It is two instructions yet it involves a large 64bit immediate for the member address.

To optimize this, I store the emulation struct address in the RSI register, and any access to it can then be performed in one mov operation using base addressing.  As RSI is callee save, I also had to implement prologue and epilogue code to save this register.  That turned out to be a win too as it simplified the call handling code.  This does have the benefit that I could reuse the same already converted code blocks when running another `hart` simply by passing in a new state structure.  It is not directly supported yet, but nice to know its an option.

At this point `DOOM` is running around 550 MIPS, which is a nice boost over the results I had this morning.  While `Quake` is less consistent in speed it is still faster than it was previously at around 282 MIPS.  This should increase a lot when I support `RV32F` in the DynRec.

The lesson I learned here is that encoding everything as immediate data is not good for instruction count or code size.  Also it pays to read the ABI spec closely, and I was caught out by a few issues such as shadow space, alignment, etc.

The list of things I am thinking about going forward:
- Code cleanup
- Linux support
- Optimizing the generated code
- Supporting RV32F in the DynRec
- Testing
- Adding user input to `Quake` and `Doom`

Just a quick note on how I generated the code fragments.  I made extensive use of an [Online Assembler](http://shell-storm.org/online/Online-Assembler-and-Disassembler) and the ever useful [Godbolt Compiler Exporer](https://godbolt.org/).  Using Godbolt I could verify what I thought the assembly my instruction emulations would lower to, which was mainly useful for navigating all the different forms of comparison instructions.  The instruction strings themselves were generated using the online assembler and could quickly be inserted into the JIT code header.

----
### 04 Oct. 2020

I have managed to progress this project far beyond my initial targets.  Over the last few days I have implemented a binary translation engine from RISC-V instructions into native x64 code.  The result of this is a hefty increase in execution speed with `DOOM` now running at ~460 MIPS compared to the emulation only 117 MIPS.  Interestingly `Quake` also sees a speed boost running at ~247 MIPS, a win over the emulation only ~103 MIPS.

The reason for the large different in MIPS between Quake and Doom is because the DynRec currently only supports the RV32I and some of the RV32IM instructions.  Any basic blocks that make use of floating point instructions will fall back to pure emulation.  As the DynRec is taught how to compile float instructions then this figure can be expected to rise significantly.

At the time of writing the DynRec engine will only run on Windows X64 machines, because it makes use of the Windows API and follows the Windows x64 calling convention for generated code. I do plan to extend support to Linux in the future too, which would however require some thought about how avoid increasing the code complexity to handle different host ABIs.

The high level operation of the DynRec can be summarized as follows:
- Look in a hashmap for a code block matching the current PC
- if a block is found
  - execute this block
- if a block is not found
  - allocate a new block
  - invoke the translator for this block
  - insert it into the hash map
  - execute this block

Translation happens at the basic block level, so each block will end after a branch instruction has been translated. Currently there is no optimization pass of the generated code, it is entirely unaware of the code surrounding it and so it is fast to generate and slow to execute.

Using the current DynRec gives us a speed boost due to the following reasons:
- No instruction fetch
- No instruction decode
- Immediate values are baked into translated instructions
  - Values of 0 can be optimized
- Zero register can be optimized
  - No lookup required
  - Writes are discarded
- Reduced emulation loop overhead
- Blocks can be chained based on previous branch pattern for faster lookup

----
### 01 Oct. 2020

Another fairly significant milestone has been achieved;  I have compiled and successfully run `Quake` on the RISC-VM.  What makes this milestone stand apart from `Doom` is that `Quake` uses floating point math in a lot of areas.  For this reason it feels like a fairly good test of the RV32F support I have been working on.

This time around I did not find any `Quake` port that would be as comparatively simple to port as `generic doom` was.  I rolled up my sleave and knocked out just such a port myself [here](https://github.com/bit-hack/portable_quake).  This involved stripping out everything that was not part of the c standard library, all use of double, undefined behavior and a vast amount of cleanup.  I used the same mechanism to display graphics that had worked for the `Doom` port, the twist being that quake used palletized graphics.

Whats really interesting is that the framerate feels around the same as for the `Doom` demo despite the generational step.  The `Quake` demo runs around 103928000 IPS (~103 MIPS) on average which is a little lower than `Doom`.  I have not investigated yet where the gap comes from or what I can learn from this.

It is interesting to see lots of the floating point instructions light up and some stay strangely dormant, for instance:
- The `sqrtf` calls do seem to get lowered to the sqrt instruction.
- The sign swapping instructions do get called.
- Min/Max never seem to get called.
- None of the FMA variants get called.
- fclass does not get called.

----
### 30 Sept. 2020

Since the last update I have managed to tidy up the SDL code used for the DOOM demo and it has been merged back into master.

At this point I started work on floating point support, which by now is reasonably functional and somewhat tested.  There is missing support for rounding and exceptions being raised in the `fcsr` register as well as verifying correct `NaN` handling.

Something on my list to look at is the `RISCV tests` repository which seemed to contain more tests than the compliance test suite.  Hopefully I can use this to iron out any bugs and missing features in the atomic and float implementations.

To test out the float implementation I added a number of tests and benchmarks to the tests folder, such as the classic `whetstone` and `linpack`.  There are some fun ones too such at the `mandelbrot` test from the always amusing `IOCCC`.

So next steps are looking like more testing, more code cleanups and perhaps another infamous (and more float oriented) demo to follow on from `Doom`.  I also need to rewrite a lot of the README since many notes and build instructions are a little out of date now.

----
### 28 Sept. 2020

I hit a milestone of sorts yesterday evening.  I was able to compile and run DOOM on the RISC-VM simulator!  I was very surprised that it was quite so easy to get up and running.  The execution speed is not quite high enough to maintain a decent framerate however.

A round of profiling was done, which showed me that the instruction fetch step was a significant bottleneck.  Some of the memory routines were then rewritten to improve their performance which brought the game up to interactive frame rates.

I have added a MIPS counter to the project today which should give me an indication of instruction throughput.  Currently when running DOOM in riscv-vm on my laptop (I7-6600U 2.60GHz) it reaches a fairly consistent 117955000 IPS (118 MIPS).  The work done per instruction would be lower than a CISC design however, which is something to keep in mind.

My work now is focused on cleaning up the code, merging my DOOM specific code back into the master branch in a clean and portable way.  Once thats done I will think about optimizing as well as adding float support.

----
### 27 Sept. 2020

It has been just over a week since I started this project and it has come a long way in that time.  I was fortunate that due to the well written ISA spec and its design that the emulator core came up fairly rapidly showing good initial correctness.  Having the compliance test suite to hand was fantastic for validating the instructions as I added them.

Once the core ISA emulator passed all of the compliance tests for the RV32I subset, I added support for the RV32M instructions.  Sadly while I have added support for the Atomic instructions I cant seem to find any way to test them, as there are currently no compliance tests.

With the core feeling fairly "stable" my focus has been on porting over executable programs to run so I can increase my confidence its working correctly.

During this process, I fell down the rabbit hole of implementing syscalls so that the virtual machine could interact with the outside world.  It is a very nice thing to see "Hello World!" printed by your own VM.

I added some simple CI jobs to the project using travis and Appveyor to cover Linux and Visual Studio builds.  A test suite of sorts will have to be created and integrated fairly soon.

While bringing up `malloc` I learned a few things about the `brk` system call along the way which were interesting, as well as conflicting with most of the information I could find.  A lot of time was spent poking through the newlib source code.

Debugging issues so far has been a pain and has mostly consisted of dumping vast logs of PC values and then manually searching through the program disassembly to see which branches were taken.  The situation improved a little when I added symbol names to the trace logs.  At some stage I will certainly have to write an unwinder.

Once malloc was working, I could move over to bringing up some of the basic `stdio` routines (`fopen`, `fwrite`, etc).  Their implementation still leaves room for improvements but it works well as a start.

I am happy so far with my design choice to keep the ISA core written in C for simplicity and ease of porting and defer to C++ for the more complex elements of the VM itself.

For the next steps I will be thinking about:
- Adding single precision `float` support.
- Running a wider corpus of programs on the VM.
- Producing a test suite to be executed by the CI jobs.
