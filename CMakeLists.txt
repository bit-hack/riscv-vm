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


set(RISCV_COMMON_SRC
    "riscv_core/riscv.h"
    "riscv_core/riscv_conf.h"
    "riscv_core/riscv_private.h"
    "riscv_core/riscv_common.cpp"
    )
add_library(riscv_common ${RISCV_COMMON_SRC})


set(RISCV_CORE_SRC
    "riscv_core/riscv.cpp"
    )
add_library(riscv_core ${RISCV_CORE_SRC})


set(RISCV_CORE_JIT_SRC
    "riscv_core/riscv_jit.cpp"
    "riscv_core/decode.h"
    "riscv_core/decode.cpp"
    "riscv_core/codegen.cpp"
    "riscv_core/emulate.cpp"
    )
add_library(riscv_core_jit ${RISCV_CORE_JIT_SRC})


set(TINYCG_SRC
    "tinycg/tinycg.cpp"
    "tinycg/tinycg.h"
    )
add_library(tinycg ${TINYCG_SRC})


set(DRV_SRC
    "riscv_vm/elf.h"
    "riscv_vm/elf.cpp"
    "riscv_vm/main.cpp"
    "riscv_vm/memory.h"
    "riscv_vm/memory.cpp"
    "riscv_vm/syscall.cpp"
    "riscv_vm/state.h"
    "riscv_vm/args.cpp"
    "riscv_vm/syscall_sdl.cpp"
    )

add_executable(riscv_vm ${DRV_SRC})
target_link_libraries(riscv_vm riscv_common riscv_core)

if (${RVVM_X64_JIT})
    add_executable(riscv_vmx ${DRV_SRC})
    target_link_libraries(riscv_vmx riscv_common riscv_core_jit tinycg)
endif()

if (${RVVM_USE_SDL})
    target_link_libraries(riscv_vm ${SDL_LIBRARY})
    if (${RVVM_X64_JIT})
        target_link_libraries(riscv_vmx ${SDL_LIBRARY})
    endif()
endif()
