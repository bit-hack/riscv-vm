cmake_minimum_required(VERSION 3.0)
project(riscv)
set(CMAKE_CXX_STANDARD 14)

option(RVM_USE_SDL "Use SDL for video and input services" OFF)
option(RVM_STRICT "Enable strict compiler warnings" OFF)

if (${RVM_USE_SDL})
    find_package(SDL REQUIRED)
    include_directories(${SDL_INCLUDE_DIR})
    add_definitions(-DRISCV_VM_USE_SDL=1)
else()
    add_definitions(-DRISCV_VM_USE_SDL=0)
endif()

if (${RVM_STRICT})
    if (MSVC)
        add_compile_options(/W4 /WX)
    else()
        add_compile_options(-Wall -Wextra -pedantic -Werror)
    endif()
endif()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(LIB_SRC
    "riscv_core/riscv.c"
    "riscv_core/riscv.h"
    "riscv_core/riscv_conf.h"
    )
add_library(riscv_core ${LIB_SRC})

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
target_link_libraries(riscv_vm riscv_core)

if (${RVM_USE_SDL})
    target_link_libraries(riscv_vm ${SDL_LIBRARY})
endif()
