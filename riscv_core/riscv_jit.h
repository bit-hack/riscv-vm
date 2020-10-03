#pragma once
#include <stdint.h>
#include <stdio.h>



#define DEBUG_JIT 0

#if DEBUG_JIT
#define JITPRINTF(...) printf(__VA_ARGS__)
#else
#define JITPRINTF(...)
#endif


struct block_t {
  // number of instructions encompased
  uint32_t instructions;
  // address range of the basic block
  uint32_t pc_start;
  uint32_t pc_end;
  // number of bytes that have been emitted
  uint32_t head;
  uint8_t *code;
};


static void gen_emit_data(struct block_t *block, uint8_t *ptr, uint32_t size) {
  // copy into the code buffer
  memcpy(block->code + block->head, ptr, size);
  block->head += size;
}

static void gen_mov_rax_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("mov rax, %u\n", imm);
  gen_emit_data(block, "\x48\xc7\xc0", 3);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_mov_rcx_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("mov rcx, %u\n", imm);
  gen_emit_data(block, "\x48\xc7\xc1", 3);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_mov_rcx_imm64(struct block_t *block, uint64_t imm) {
  JITPRINTF("mov r8, %llx\n", imm);
  gen_emit_data(block, "\x48\xb9", 2);
  gen_emit_data(block, (uint8_t*)&imm, 8);
}

static void gen_cmp_rax_rcx(struct block_t *block) {
  JITPRINTF("cmp rax, rcx\n");
  gen_emit_data(block, "\x48\x39\xc8", 3);
}

static void gen_xor_rax_rax(struct block_t *block) {
  JITPRINTF("xor rax, rax\n");
  gen_emit_data(block, "\x48\x31\xc0", 3);
}

static void gen_cmp_rax_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("cmp rax, %d\n", (int32_t)imm);
  gen_emit_data(block, "\x48\x3d", 2);
  gen_emit_data(block, (uint8_t*)&imm, 2);
}

static void gen_mov_r8_imm64(struct block_t *block, uint64_t imm) {
  JITPRINTF("mov r8, %llx\n", imm);
  gen_emit_data(block, "\x49\xb8", 2);
  gen_emit_data(block, (uint8_t*)&imm, 8);
}

static void gen_mov_r8_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("mov r8, %02x\n", imm);
  gen_emit_data(block, "\x49\xc7\xc0", 3);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_mov_r9_imm64(struct block_t *block, uint64_t imm) {
  JITPRINTF("mov r9, %llx\n", imm);
  gen_emit_data(block, "\x49\xb9", 2);
  gen_emit_data(block, (uint8_t*)&imm, 8);
}

static void gen_call_r9(struct block_t *block) {
  // the caller must allocate space for 4 arguments on the stack prior to
  // calling.
  JITPRINTF("sub rsp, 32\n");
  gen_emit_data(block, "\x48\x83\xec\x20", 4);
  JITPRINTF("call r9\n");
  gen_emit_data(block, "\x41\xff\xd1", 3);
  JITPRINTF("add rsp, 32\n");
  gen_emit_data(block, "\x48\x83\xc4\x20", 4);
}

static void gen_add_rdx_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("add rdx, %d\n", (int32_t)imm);
  gen_emit_data(block, "\x48\x81\xc2", 3);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_mov_ecx_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("mov ecx, %u\n", imm);
  gen_emit_data(block, "\xb9", 1);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_mov_eax_rv32pc(struct block_t *block, struct riscv_t *rv) {
  JITPRINTF("mov r11, &rv->PC\n");
  gen_emit_data(block, "\x49\xbb", 2);
  uintptr_t ptr = (uintptr_t)&rv->PC;
  gen_emit_data(block, (uint8_t*)&ptr, 8);
  JITPRINTF("mov eax, [r11]\n");
  gen_emit_data(block, "\x41\x8b\x03", 3);
}

static void gen_mov_rv32pc_eax(struct block_t *block, struct riscv_t *rv) {
  JITPRINTF("mov r11, &rv->PC\n");
  gen_emit_data(block, "\x49\xbb", 2);
  uintptr_t ptr = (uintptr_t)&rv->PC;
  gen_emit_data(block, (uint8_t*)&ptr, 8);
  JITPRINTF("mov [r11], eax\n");
  gen_emit_data(block, "\x41\x89\x03", 3);
}

static void gen_mov_eax_rv32reg(struct block_t *block, struct riscv_t *rv, uint32_t reg) {
  if (reg == rv_reg_zero) {
    JITPRINTF("xor eax, eax\n");
    gen_emit_data(block, "\x31\xc0", 2);
  }
  else {
    JITPRINTF("mov r11, &rv->X[%u]\n", reg);
    gen_emit_data(block, "\x49\xbb", 2);
    uintptr_t ptr = (uintptr_t)&rv->X[reg];
    gen_emit_data(block, (uint8_t*)&ptr, 8);
    JITPRINTF("mov eax, [r11]\n");
    gen_emit_data(block, "\x41\x8b\x03", 3);
  }
}

static void gen_mov_ecx_rv32reg(struct block_t *block, struct riscv_t *rv, uint32_t reg) {
  if (reg == rv_reg_zero) {
    JITPRINTF("xor ecx, ecx\n");
    gen_emit_data(block, "\x31\xc9", 2);
  }
  else {
    JITPRINTF("mov r11, &rv->PC\n");
    gen_emit_data(block, "\x49\xbb", 2);
    uintptr_t ptr = (uintptr_t)&rv->X[reg];
    gen_emit_data(block, (uint8_t*)&ptr, 8);
    JITPRINTF("mov ecx, [r11]\n");
    gen_emit_data(block, "\x41\x8b\x0b", 3);
  }
}

static void gen_mov_rv32reg_eax(struct block_t *block, struct riscv_t *rv, uint32_t reg) {
  if (reg != rv_reg_zero) {
    JITPRINTF("mov r11, &rv->X[%u]\n", reg);
    gen_emit_data(block, "\x49\xbb", 2);
    uintptr_t ptr = (uintptr_t)&rv->X[reg];
    gen_emit_data(block, (uint8_t*)&ptr, 8);
    JITPRINTF("mov [r11], eax\n");
    gen_emit_data(block, "\x41\x89\x03", 3);
  }
}

static void gen_add_eax_ecx(struct block_t *block) {
  JITPRINTF("add eax, ecx\n");
  gen_emit_data(block, "\x01\xc8", 2);
}

static void gen_add_eax_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("add eax, %02x\n", imm);
  gen_emit_data(block, "\x05", 1);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_xor_eax_eax(struct block_t *block) {
  JITPRINTF("xor eax, eax\n");
  gen_emit_data(block, "\x31\xc0", 2);
}

static void gen_xor_eax_ecx(struct block_t *block) {
  JITPRINTF("xor eax, ecx\n");
  gen_emit_data(block, "\x31\xc8", 2);
}

static void gen_and_eax_ecx(struct block_t *block) {
  JITPRINTF("and eax, ecx\n");
  gen_emit_data(block, "\x21\xc8", 2);
}

static void gen_or_eax_ecx(struct block_t *block) {
  JITPRINTF("or eax, ecx\n");
  gen_emit_data(block, "\x09\xc8", 2);
}

static void gen_sub_eax_ecx(struct block_t *block) {
  JITPRINTF("sub eax, ecx\n");
  gen_emit_data(block, "\x29\xc8", 2);
}

static void gen_xor_eax_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("xor eax, %02x\n", imm);
  gen_emit_data(block, "\x35", 1);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_or_eax_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("or eax, %02x\n", imm);
  gen_emit_data(block, "\x0d", 1);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_and_eax_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("and eax, %02x\n", imm);
  gen_emit_data(block, "\x25", 1);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_cmp_eax_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("cmp eax, %02x\n", imm);
  gen_emit_data(block, "\x3d", 1);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_mov_eax_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("mov eax, %02u\n", imm);
  gen_emit_data(block, "\xb8", 1);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_cmp_eax_ecx(struct block_t *block) {
  JITPRINTF("cmp eax, ecx\n");
  gen_emit_data(block, "\x39\xc8", 2);
}

static void gen_mov_rv32pc_r8(struct block_t *block, struct riscv_t *rv) {
  JITPRINTF("mov r11, &rv->PC\n");
  gen_emit_data(block, "\x49\xbb", 2);
  uintptr_t ptr = (uintptr_t)&rv->PC;
  gen_emit_data(block, (uint8_t*)&ptr, 8);
  JITPRINTF("mov [r11], r8\n");
  gen_emit_data(block, "\x4d\x89\x03", 3);
}

static void gen_mov_r8_rv32reg(struct block_t *block, struct riscv_t *rv, uint32_t reg) {
  if (reg == rv_reg_zero) {
    JITPRINTF("xor r8, r8\n");
    gen_emit_data(block, "\x4d\x31\xc0", 3);
  }
  else {
    JITPRINTF("mov r11, &rv->X[%u]\n", reg);
    gen_emit_data(block, "\x49\xbb", 2);
    uintptr_t ptr = (uintptr_t)&rv->X[reg];
    gen_emit_data(block, (uint8_t*)&ptr, 8);
    JITPRINTF("mov r8, [r11]\n");
    gen_emit_data(block, "\x4d\x8b\x03", 3);
  }
}

static void gen_mov_edx_rv32reg(struct block_t *block, struct riscv_t *rv, uint32_t reg) {
  if (reg == rv_reg_zero) {
    JITPRINTF("xor edx, edx\n");
    gen_emit_data(block, "\x31\xd2", 2);
  }
  else {
    JITPRINTF("mov r11, &rv->X[%u]\n", reg);
    gen_emit_data(block, "\x49\xbb", 2);
    uintptr_t ptr = (uintptr_t)&rv->X[reg];
    gen_emit_data(block, (uint8_t*)&ptr, 8);
    JITPRINTF("mov edx, [r11]\n");
    gen_emit_data(block, "\x41\x8b\x13", 3);
  }
}

static void gen_xor_rdx_rdx(struct block_t *block) {
  JITPRINTF("xor rdx, rdx\n");
  gen_emit_data(block, "\x48\x31\xd2", 3);
}

static void gen_add_edx_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("add edx, %02x\n", imm);
  gen_emit_data(block, "\x81\xc2", 2);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_and_cl_imm8(struct block_t *block, uint8_t imm) {
  JITPRINTF("and cl, %02x\n", (int32_t)imm);
  gen_emit_data(block, "\x80\xe1", 2);
  gen_emit_data(block, &imm, 1);
}

static void gen_shl_eax_cl(struct block_t *block) {
  JITPRINTF("shl eax, cl\n");
  gen_emit_data(block, "\xd3\xe0", 2);
}

static void gen_sar_eax_cl(struct block_t *block) {
  JITPRINTF("sar eax, cl\n");
  gen_emit_data(block, "\xd3\xf8", 2);
}

static void gen_setb_dl(struct block_t *block) {
  JITPRINTF("setb dl\n");
  gen_emit_data(block, "\x0f\x92\xc2", 3);
}

static void gen_setl_dl(struct block_t *block) {
  JITPRINTF("setl dl\n");
  gen_emit_data(block, "\x0f\x9c\xc2", 3);
}

static void gen_shr_eax_imm8(struct block_t *block, uint8_t imm) {
  JITPRINTF("shr eax, %d\n", (int32_t)imm);
  gen_emit_data(block, "\xc1\xe8", 2);
  gen_emit_data(block, &imm, 1);
}

static void gen_sar_eax_imm8(struct block_t *block, uint8_t imm) {
  JITPRINTF("shr eax, %d\n", (int32_t)imm);
  gen_emit_data(block, "\xc1\xf8", 2);
  gen_emit_data(block, &imm, 1);
}

static void gen_shl_eax_imm8(struct block_t *block, uint8_t imm) {
  JITPRINTF("shl eax, %d\n", (int32_t)imm);
  gen_emit_data(block, "\xc1\xe0", 2);
  gen_emit_data(block, &imm, 1);
}

static void gen_movsx_eax_al(struct block_t *block) {
  JITPRINTF("movsx eax, al\n");
  gen_emit_data(block, "\x0f\xbe\xc0", 3);
}

static void gen_movsx_eax_ax(struct block_t *block) {
  JITPRINTF("movsx eax, ax\n");
  gen_emit_data(block, "\x0f\xbf\xc0", 3);
}

static void gen_mov_edx_imm32(struct block_t *block, uint32_t imm) {
  JITPRINTF("mov edx, %02x\n", imm);
  gen_emit_data(block, "\xba", 1);
  gen_emit_data(block, (uint8_t*)&imm, 4);
}

static void gen_cmove_eax_edx(struct block_t *block) {
  JITPRINTF("cmove eax, edx\n");
  gen_emit_data(block, "\x0f\x44\xc2", 3);
}

static void gen_cmovne_eax_edx(struct block_t *block) {
  JITPRINTF("cmovne eax, edx\n");
  gen_emit_data(block, "\x0f\x45\xc2", 3);
}

static void gen_cmovl_eax_edx(struct block_t *block) {
  JITPRINTF("cmovl eax, edx\n");
  gen_emit_data(block, "\x0f\x4c\xc2", 3);
}

static void gen_cmovge_eax_edx(struct block_t *block) {
  JITPRINTF("cmovge eax, edx\n");
  gen_emit_data(block, "\x0f\x4d\xc2", 3);
}

static void gen_cmovb_eax_edx(struct block_t *block) {
  JITPRINTF("cmovb eax, edx\n");
  gen_emit_data(block, "\x0f\x42\xc2", 3);
}

static void gen_cmovnb_eax_edx(struct block_t *block) {
  JITPRINTF("cmovnb eax, edx\n");
  gen_emit_data(block, "\x0f\x43\xc2", 3);
}

static void gen_ret(struct block_t *block) {
  JITPRINTF("ret\n");
  gen_emit_data(block, "\xc3", 1);
}

static void gen_xor_edx_edx(struct block_t *block) {
  JITPRINTF("xor edx, edx\n");
  gen_emit_data(block, "\x31\xd2", 2);
}

static void gen_mov_eax_edx(struct block_t *block) {
  JITPRINTF("mov eax, edx\n");
  gen_emit_data(block, "\x89\xd0", 2);
}

static void gen_shr_eax_cl(struct block_t *block) {
  JITPRINTF("shr eax, cl\n");
  gen_emit_data(block, "\xd3\xe8", 2);
}

static void gen_movzx_eax_dl(struct block_t *block) {
  JITPRINTF("movzx eax, dl\n");
  gen_emit_data(block, "\x0f\xb6\xc2", 3);
}
