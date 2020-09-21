// build using:
//   riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 main.c -o helloworld

#include <stdio.h>

int main(int argc, const char **args) {
  printf("Hello World!\n");
  return 0;
}
