cmake_minimum_required(VERSION 3.0)
project(riscv)
set(CMAKE_CXX_STANDARD 14)

option(RVVM_STRICT "Enable strict compiler warnings" OFF)

option(RVVM_X64_JIT "Enable x64 JIT" OFF)
if (${RVVM_X64_JIT})
    add_definitions(-DRISCV_VM_X64_JIT=1)
else()
    add_definitions(-DRISCV_VM_X64_JIT=0)
endif()

option(RVVM_SUPPORT_RV32M "Enable RV32M ISA" ON)
if (${RVVM_SUPPORT_RV32M})
    add_definitions(-DRISCV_VM_SUPPORT_RV32M=1)
else()
    add_definitions(-DRISCV_VM_SUPPORT_RV32M=0)
endif()

option(RVVM_SUPPORT_RV32F "Enable RV32F ISA" ON)
if (${RVVM_SUPPORT_RV32F})
    add_definitions(-DRISCV_VM_SUPPORT_RV32F=1)
else()
    add_definitions(-DRISCV_VM_SUPPORT_RV32F=0)
endif()

option(RVVM_SUPPORT_RV32A "Enable RV32A ISA" ON)
if (${RVVM_SUPPORT_RV32A})
    add_definitions(-DRISCV_VM_SUPPORT_RV32A=1)
else()
    add_definitions(-DRISCV_VM_SUPPORT_RV32A=0)
endif()

option(RVVM_SUPPORT_RV32Zicsr "Enable Zicsr ISA" ON)
if (${RVVM_SUPPORT_RV32Zicsr})
    add_definitions(-DRISCV_VM_SUPPORT_Zicsr=1)
else()
    add_definitions(-DRISCV_VM_SUPPORT_Zicsr=0)
endif()

option(RVVM_SUPPORT_RV32Zifencei "Enable Zifencei ISA" ON)
if (${RVVM_SUPPORT_RV32M})
    add_definitions(-DRISCV_VM_SUPPORT_Zifencei=1)
else()
    add_definitions(-DRISCV_VM_SUPPORT_Zifencei=0)
endif()

option(RVVM_USE_SDL "Use SDL for video and input services" OFF)
if (${RVVM_USE_SDL})
    find_package(SDL REQUIRED)
    include_directories(${SDL_INCLUDE_DIR})
    add_definitions(-DRISCV_VM_USE_SDL=1)
else()
    add_definitions(-DRISCV_VM_USE_SDL=0)
endif()

if (${RVVM_STRICT})
    if (MSVC)
        add_compile_options(/W4 /WX)
    else()
        add_compile_options(-Wall -Wextra -pedantic -Werror)
    endif()
endif()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(RISCV_EMU_SRC
    "riscv_emu/riscv.c"
    "riscv_emu/riscv.h"
    "riscv_emu/riscv_conf.h"
    "riscv_emu/riscv_private.h"
    "riscv_emu/riscv_common.c"
    )
add_library(riscv_emu ${RISCV_EMU_SRC})

set(RISCV_NOJIT_SRC
    "riscv_nojit/riscv_nojit.c"
    )
add_library(riscv_nojit ${RISCV_NOJIT_SRC})

set(RISCV_JIT_1_SRC
    "riscv_jit_1/riscv_jit_1.c"
    )
add_library(riscv_jit_1 ${RISCV_JIT_1_SRC})

set(RISCV_JIT_2_SRC
    "riscv_jit_2/riscv_jit_2.h"
    "riscv_jit_2/riscv_jit_2.c"
    "riscv_jit_2/ir.h"
    "riscv_jit_2/ir.c"
    )
add_library(riscv_jit_2 ${RISCV_JIT_2_SRC})

set(TINYCG_SRC
    "tinycg/tinycg.c"
    "tinycg/tinycg.h"
    )
add_library(tinycg ${TINYCG_SRC})

set(DRV_SRC
    "riscv_vm/elf.h"
    "riscv_vm/elf.cpp"
    "riscv_vm/main.cpp"
    "riscv_vm/memory.h"
    "riscv_vm/syscall.cpp"
    "riscv_vm/state.h"
    "riscv_vm/args.cpp"
    "riscv_vm/syscall_sdl.cpp"
    )

add_executable(riscv_vm ${DRV_SRC})
target_link_libraries(riscv_vm riscv_emu riscv_nojit)

add_executable(riscv_vm_j1 ${DRV_SRC})
target_link_libraries(riscv_vm_j1 riscv_emu tinycg riscv_jit_1)

add_executable(riscv_vm_j2 ${DRV_SRC})
target_link_libraries(riscv_vm_j2 riscv_emu riscv_jit_2)

if (${RVVM_USE_SDL})
    target_link_libraries(riscv_vm ${SDL_LIBRARY})
    target_link_libraries(riscv_vm_j1 ${SDL_LIBRARY})
    target_link_libraries(riscv_vm_j2 ${SDL_LIBRARY})
endif()
