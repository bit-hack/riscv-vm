# RISCV-VM News

----

Some time was spent today looking at the largest bottlenecks in the generated code which were in riscv register file access and callbacks when doing io and syscalls.  Both of these operations would involve accessing the members of a structure which stores the state of the emulation.  The DynRec would place the address of these members into a register and then perform a write or read from that address.  It is two instructions yet it involves a large 64bit immediate for the member address.

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

----
### 04 Oct. 2020

I have managed to progress this project far beyond my initial targets.  Over the last few days I have implemented a binary translation engine from RISCV instructions into native x64 code.  The result of this is a hefty increase in execution speed with `DOOM` now running at ~460 MIPS compared to the emulation only 117 MIPS.  Interestingly `Quake` also sees a speed boost running at ~247 MIPS, a win over the emulation only ~103 MIPS.

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
