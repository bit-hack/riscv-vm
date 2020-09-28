# RISV-VM News

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
