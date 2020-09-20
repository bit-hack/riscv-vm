cmake_minimum_required(VERSION 3.0)
project(riscv)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

file(GLOB LIB_SRC
    "riscv_emu/*.c"
    "riscv_emu/*.h")
add_library(riscv_emu ${LIB_SRC})

file(GLOB DRV_SRC
    "riscv_driver/*.cpp"
    "riscv_driver/*.h")
add_executable(riscv_driver ${DRV_SRC})
target_link_libraries(riscv_driver riscv_emu)
