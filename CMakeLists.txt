cmake_minimum_required(VERSION 3.0)
project(riscv)
set(CMAKE_CXX_STANDARD 14)

find_package(SDL REQUIRED)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
include_directories(${SDL_INCLUDE_DIR})

set(LIB_SRC
    "riscv_core/riscv.c"
    "riscv_core/riscv.h"
    )
add_library(riscv_core ${LIB_SRC})

set(DRV_SRC
    "riscv_vm/elf.h"
    "riscv_vm/elf.cpp"
    "riscv_vm/main.cpp"
    "riscv_vm/memory.h"
    "riscv_vm/syscall.cpp"
    "riscv_vm/syscall_doom.cpp"
    "riscv_vm/state.h"
    "riscv_vm/args.cpp"
    )
add_executable(riscv_vm ${DRV_SRC})
target_link_libraries(riscv_vm riscv_core ${SDL_LIBRARY})
