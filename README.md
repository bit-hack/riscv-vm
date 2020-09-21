# RISC-V Virtual Machine

This is a simple RISCV-V Virtual Machine and instruction set emulator implementing the RV32I processor model.

There are two elements to this project:
- An ISA emulator written in C which presents a low level API for interfacing.
- A frontend written in C++ which interfaces the ISA emulator with the rest of the world.

The key goals of this project is to provide a small emulator for learning about RISC-V with a simple API for embedding it in other projects.


----
## Build requirements
- CMake 3.0 or above


----
## Compiling the emulator
```
mkdir build
cd build
cmake ..
make
```


----
## Executing a basic program

A simple RISC-V program (in `main.c`) can be compiled and executed as follows:
```
riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 main.c
riscv_driver a.out
```


----
## Newlib support

There is rudimentary support for target programs that make use of the newlib library.
Currently `_exit` and `_write` syscalls have been implemented via the `ecall` instruction.  This is just enough to run a simple "hello world" program.

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