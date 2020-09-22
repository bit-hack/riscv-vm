cmake_minimum_required(VERSION 3.0)
project(riscv)
set(CMAKE_CXX_STANDARD 14)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(LIB_SRC
    "riscv_core/riscv.c"
    "riscv_core/riscv.h")
add_library(riscv_core ${LIB_SRC})

set(DRV_SRC
    "riscv_vm/elf.h"
    "riscv_vm/file.h"
    "riscv_vm/main.cpp"
    "riscv_vm/memory.h")
add_executable(riscv_vm ${DRV_SRC})
target_link_libraries(riscv_vm riscv_core)
