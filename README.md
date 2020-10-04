# RISC-V Virtual Machine

This is a RISCV-V Virtual Machine and instruction set emulator implementing a 32 bit RISCV-V processor model.  I started this project as a learning exercise to get more familiar with the RISC-V eco system and have increased the scope of the project as it matures.  The project itself is still very much in the early stages however.

Features:
- Support for RV32I and RV32M
- Partial support for RV32F and RV32A
- Syscall emulation and host passthrough
- Emulation using [Dynamic Binary Translation](https://en.wikipedia.org/wiki/Binary_translation#Dynamic_binary_translation)
- It can run Doom, Quake and SmallPT

There are broadly two elements to this project:
- An ISA emulator core written in C which presents a low level API for interfacing.
- The VM frontend written in C++ which interfaces the ISA emulator core with the host computer.

Note: The Binary Translation emulator is currently only available when building for x64 Windows. This is due to the generated code being tailored to that ABI, however in time Linux support for the code generator will follow.

See [news](NEWS.md) for a development log and updates.


----
## Build Status
[![Build Status](https://travis-ci.org/bit-hack/riscv-vm.svg?branch=master)](https://travis-ci.org/bit-hack/riscv-vm)
[![Build status](https://ci.appveyor.com/api/projects/status/sxu9jv014g179mus/branch/master?svg=true)](https://ci.appveyor.com/project/8BitPimp/riscv-vm/branch/master)


----
## Build requirements
- C++ 14 compatible compiler  (VisualStudio 15+, GCC 4.9+)
- CMake 3.0 or above
- SDL1.2 (if you want video support)


----
## Compiling the project
```
mkdir build
cd build
cmake ..
make
```


----
## Executing a basic program

A simple RISC-V program (i.e. `main.c`) can be compiled and executed as follows:
```
riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 main.c
riscv_vm a.out
```


----
## Testing
Please note that while the riscv-vm simulator is provided under the MIT license, any of the materials in the `tests` folder may not be.
These tests were taken from a wide range of places to give the project a reasonable test coverage.
If you are the author of any of these tests and are unhappy with its inclusion here then please get in touch and I will remove it from the project and its git history.


----
## Notes

- Written referencing the `20191214-draft` Volume I: Unprivileged ISA document
- Complete RV32G compliance is the end goal
- Testing is done currently using a private fork of the compliance test suite
  - As the project develops testing will become a key focus
- It is enough that it can run doom, see [the README](tests/doom/README.md)


----
## Newlib support

There is rudimentary support for target programs that make use of the newlib library.
Currently a number of syscalls have been implemented via the `ecall` instruction.
This is just enough to run a number of simpler programs and do some basic file io operations.

Try the following example program:

```C
#include <stdio.h>

int main(int argc, const char **args) {
  printf("Hello World!\n");
  return 0;
}
```

Spoiler:
```
Hello World!
```