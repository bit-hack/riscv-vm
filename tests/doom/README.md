# RISCV-VM DOOM

This is a highly experimental build of DOOM for the riscv-vm emulator.
It is an interesting proof of concept however the emulator runs too slowly to play it currently.
While DOOM will serve as a great testbed for profiling it goes to show that some kind of dynrec will be needed in the future.

Currently only rendering has been implemented and not any user input so you can only watch the recorded demo.
I will surely add user input in time as I improve the emulator speed too.


### Prerequisites
- SDL1.2 Library


### Build with SDL Instructions
```
mkdir build
cd build
cmake .. -DUSE_SDL
make
```


### Instructions
- Build the `riscv-vm` project with SDL support.
- Download the shareware `doom1.wad` file.
- Place `doom1.wad` into the same folder as `doom.elf`.
- From the `tests/doom` folder run `./riscv-vm doom.elf`.
