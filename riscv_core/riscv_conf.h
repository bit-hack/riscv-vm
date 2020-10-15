#pragma once

// enable RV32M
#ifndef RISCV_VM_SUPPORT_RV32M
#define RISCV_VM_SUPPORT_RV32M     1
#endif
// enable RV32 Zicsr
#ifndef RISCV_VM_SUPPORT_Zicsr
#define RISCV_VM_SUPPORT_Zicsr     1
#endif
// enable RV32 Zifencei
#ifndef RISCV_VM_SUPPORT_Zifencei
#define RISCV_VM_SUPPORT_Zifencei  1
#endif
// enable RV32A
#ifndef RISCV_VM_SUPPORT_RV32A
#define RISCV_VM_SUPPORT_RV32A     1
#endif
// enable RV32F
#ifndef RISCV_VM_SUPPORT_RV32F
#define RISCV_VM_SUPPORT_RV32F     1
#endif
// enable x64 JIT
#ifndef RISCV_VM_X64_JIT
#define RISCV_VM_X64_JIT           0
#endif
// enable machine mode support
#ifndef RISCV_SUPPORT_MACHINE
#define RISCV_SUPPORT_MACHINE      0
#endif

#define RISCV_DUMP_JIT_TRACE       0
#define RISCV_JIT_PROFILE          0

// default top of stack address
#define DEFAULT_STACK_ADDR (0xFFFFF000)
