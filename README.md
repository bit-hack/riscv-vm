# RISC-V Virtual Machine

This is a simple RISCV-V Virtual Machine and instruction set emulator implementing the RV32IM processor model.

There are two elements to this project:
- An ISA emulator core written in C which presents a low level API for interfacing.
- The VM frontend written in C++ which interfaces the ISA emulator core with the rest of the world.

The key goals of this project is to provide a small emulator for learning about RISC-V with a simple API for embedding it in other projects.

See [news](NEWS.md) for development updates.

See [this readme](tests/doom/README.md) for notes about the DOOM demo.


----
## Build Status
[![Build Status](https://travis-ci.org/bit-hack/riscv-vm.svg?branch=master)](https://travis-ci.org/bit-hack/riscv-vm)
[![Build status](https://ci.appveyor.com/api/projects/status/sxu9jv014g179mus/branch/master?svg=true)](https://ci.appveyor.com/project/8BitPimp/riscv-vm/branch/master)


----
## Build requirements
- C++ 14 compatable compiler  (VisualStudio 15+, GCC 4.9+)
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