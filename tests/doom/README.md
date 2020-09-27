# RISCV-VM DOOM

This is a highly experimental build of DOOM for the riscv-vm emulator.
It is an interesting proof of concept however the emulator runs too slowly to play it currently.
While DOOM will serve as a great testbed for profiling it goes to show that some kind of dynrec will be needed in the future.

Currently only rendering has been implemented and not any user input so you can only watch the recorded demo.
I will surely add user input in time as I improve the emulator speed too.


### Prerequisites
- SDL1.2
- The `aidan/doom` branch of the `riscv-vm` project.


### Instructions
- Download the shareware `doom1.wad` file.
- Place `doom1.wad` into the same folder as `doom.elf`.
- Build the `aidan/doom` branch of the `riscv-vm` project.
- From the `tests/doom` folder run `./riscv-vm doom.elf`.
