# RISV-VM News

----
### 27 Sept 2020

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
