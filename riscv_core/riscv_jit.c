#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <Windows.h>

#include "riscv.h"
#include "riscv_private.h"
#include "riscv_jit.h"


//  Windows X64 calling convention
//
//  args:
//    1   rcx
//    2   rdx
//    3   r8
//    4   r9
//
//  volatile registers:
//    rax, rcx, rdx, r8, r9, r10, r11
//

// a hash function is used when mapping addresses to indexes in the block map
static uint32_t wang_hash(uint32_t a) {
  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * 0x27d4eb2d;
  a = a ^ (a >> 15);
  return a;
}

// allocate a new code block
struct block_t *block_alloc(struct riscv_jit_t *jit) {
  // place a new block
  struct block_t *block = (struct block_t *)jit->head;
  // set the initial block write head
  block->head = 0;
  return block;
}

// finialize a code block and insert into the block map
void block_finish(struct riscv_jit_t *jit, struct block_t *block) {
  assert(jit && block && jit->head && jit->block_map);
  // advance the block head ready for the next alloc
  jit->head = block->code + block->head;
  // insert into the block map
  uint32_t index = wang_hash(block->pc_start);
  const uint32_t mask = jit->block_map_size - 1;
  for (;; ++index) {
    if (jit->block_map[index & mask] == NULL) {
      jit->block_map[index & mask] = block;
      return;
    }
  }

  // flush the instructon cache for this block
  FlushInstructionCache(GetCurrentProcess(), block->code, block->head);
}

// try to locate an already translated block in the block map
struct block_t *block_find(struct riscv_jit_t *jit, uint32_t addr) {
  assert(jit && jit->block_map);
  uint32_t index = wang_hash(addr);
  const uint32_t mask = jit->block_map_size - 1;
  for (;; ++index) {
    struct block_t *block = jit->block_map[index & mask];
    if (block == NULL) {
      return NULL;
    }
    if (block->pc_start == addr) {
      return block;
    }
  }
}

static bool op_load(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // itype format
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rd     = dec_rd(inst);

  // skip writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    block->pc_end += 4;
    return true;
  }

  // move rv into arg1
  gen_mov_rcx_imm64(block, (uint64_t)rv);

  // move load address into arg 2
  // rdx = rv->X[rs1] + imm;
  gen_mov_edx_rv32reg(block, rv, rs1);
  gen_add_edx_imm32(block, imm);

  // dispatch by read size
  switch (funct3) {
  case 0: // LB
    // rv->X[rd] = sign_extend_b(rv->io.mem_read_b(rv, addr));
    {
      gen_mov_r9_imm64(block, (uint64_t)rv->io.mem_read_b);
      gen_call_r9(block);
      gen_movsx_eax_al(block);
    }
    break;
  case 1: // LH
    // rv->X[rd] = sign_extend_h(rv->io.mem_read_s(rv, addr));
    {
      gen_mov_r9_imm64(block, (uint64_t)rv->io.mem_read_s);
      gen_call_r9(block);
      gen_movsx_eax_ax(block);
    }
    break;
  case 2: // LW
    // rv->X[rd] = rv->io.mem_read_w(rv, addr);
    {
      gen_mov_r9_imm64(block, (uint64_t)rv->io.mem_read_w);
      gen_call_r9(block);
  }
    break;
  case 4: // LBU
    // rv->X[rd] = rv->io.mem_read_b(rv, addr);
    {
      gen_mov_r9_imm64(block, (uint64_t)rv->io.mem_read_b);
      gen_call_r9(block);
  }
    break;
  case 5: // LHU
    // rv->X[rd] = rv->io.mem_read_s(rv, addr);
    {
      gen_mov_r9_imm64(block, (uint64_t)rv->io.mem_read_s);
      gen_call_r9(block);
  }
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = rax
  gen_mov_rv32reg_eax(block, rv, rd);
  // step over instruction
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_op_imm(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    block->pc_end += 4;
    return true;
  }

  // eax = rv->X[rs1]
  gen_mov_eax_rv32reg(block, rv, rs1);

  // dispatch operation type
  switch (funct3) {
  case 0: // ADDI
    // rv->X[rd] = (int32_t)(rv->X[rs1]) + imm;
    gen_add_eax_imm32(block, imm);
    break;
  case 1: // SLLI
    // rv->X[rd] = rv->X[rs1] << (imm & 0x1f);
    gen_shl_eax_imm8(block, imm & 0x1f);
    break;
  case 2: // SLTI
    // rv->X[rd] = ((int32_t)(rv->X[rs1]) < imm) ? 1 : 0;
    gen_cmp_eax_imm32(block, imm);
    gen_setl_dl(block); // signed
    gen_movzx_eax_dl(block);
    break;
  case 3: // SLTIU
    // rv->X[rd] = (rv->X[rs1] < (uint32_t)imm) ? 1 : 0;
    gen_cmp_eax_imm32(block, imm);
    gen_setb_dl(block); // unsigned
    gen_movzx_eax_dl(block);
    break;
  case 4: // XORI
    // rv->X[rd] = rv->X[rs1] ^ imm;
    gen_xor_eax_imm32(block, imm);
    break;
  case 5:
    if (imm & ~0x1f) {
      // SRAI
      // rv->X[rd] = ((int32_t)rv->X[rs1]) >> (imm & 0x1f);
      gen_sar_eax_imm8(block, imm & 0x1f);
    }
    else {
      // SRLI
      // rv->X[rd] = rv->X[rs1] >> (imm & 0x1f);
      gen_shr_eax_imm8(block, imm & 0x1f);
    }
    break;
  case 6: // ORI
    // rv->X[rd] = rv->X[rs1] | imm;
    gen_or_eax_imm32(block, imm);
    break;
  case 7: // ANDI
    // rv->X[rd] = rv->X[rs1] & imm;
    gen_and_eax_imm32(block, imm);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = eax
  gen_mov_rv32reg_eax(block, rv, rd);
  // step over instruction
  block->pc_end += 4;
  // cant branch
  return true;
}

// add upper immediate to pc
static bool op_auipc(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // the effective current PC
  const uint32_t pc = block->pc_end;
  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t imm = dec_utype_imm(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    block->pc_end += 4;
    return true;
  }

  // rv->X[rd] = imm + rv->PC;
  gen_mov_eax_imm32(block, pc + imm);
  gen_mov_rv32reg_eax(block, rv, rd);

  // step over instruction
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_store(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // s-type format
  const int32_t  imm    = dec_stype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct3 = dec_funct3(inst);

  // arg1
  gen_mov_rcx_imm64(block, (uint64_t)rv);

  // arg2
  // const uint32_t addr = rv->X[rs1] + imm;
  gen_xor_rdx_rdx(block);
  gen_mov_edx_rv32reg(block, rv, rs1);
  gen_add_edx_imm32(block, imm);

  // arg3
  // const uint32_t data = rv->X[rs2];
  gen_mov_r8_rv32reg(block, rv, rs2);

  // dispatch by write size
  switch (funct3) {
  case 0: // SB
    // rv->io.mem_write_b(rv, addr, data);
    gen_mov_r9_imm64(block, (uint64_t)rv->io.mem_write_b);
    gen_call_r9(block);
    break;
  case 1: // SH
    // rv->io.mem_write_s(rv, addr, data);
    gen_mov_r9_imm64(block, (uint64_t)rv->io.mem_write_s);
    gen_call_r9(block);
    break;
  case 2: // SW
    // rv->io.mem_write_w(rv, addr, data);
    gen_mov_r9_imm64(block, (uint64_t)rv->io.mem_write_w);
    gen_call_r9(block);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // step over instruction
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_op(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // r-type decode
  const uint32_t rd     = dec_rd(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct7 = dec_funct7(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    block->pc_end += 4;
    return true;
  }

  // get operands
  gen_mov_eax_rv32reg(block, rv, rs1);
  gen_mov_ecx_rv32reg(block, rv, rs2);

  switch (funct7) {
  case 0b0000000:
    switch (funct3) {
    case 0b000: // ADD
      // rv->X[rd] = (int32_t)(rv->X[rs1]) + (int32_t)(rv->X[rs2]);
      gen_add_eax_ecx(block);
      break;
    case 0b001: // SLL
      // rv->X[rd] = rv->X[rs1] << (rv->X[rs2] & 0x1f);
      gen_and_cl_imm8(block, 0x1f);
      gen_shl_eax_cl(block);
      break;
    case 0b010: // SLT
      // rv->X[rd] = ((int32_t)(rv->X[rs1]) < (int32_t)(rv->X[rs2])) ? 1 : 0;
      gen_cmp_eax_ecx(block);
      gen_setl_dl(block); // signed
      gen_movzx_eax_dl(block);
      break;
    case 0b011: // SLTU
      // rv->X[rd] = (rv->X[rs1] < rv->X[rs2]) ? 1 : 0;
      gen_cmp_eax_ecx(block);
      gen_setb_dl(block); // unsigned
      gen_movzx_eax_dl(block);
      break;
    case 0b100: // XOR
      // rv->X[rd] = rv->X[rs1] ^ rv->X[rs2];
      gen_xor_eax_ecx(block);
      break;
    case 0b101: // SRL
      // rv->X[rd] = rv->X[rs1] >> (rv->X[rs2] & 0x1f);
      gen_and_cl_imm8(block, 0x1f);
      gen_shr_eax_cl(block);
      break;
    case 0b110: // OR
      // rv->X[rd] = rv->X[rs1] | rv->X[rs2];
      gen_or_eax_ecx(block);
      break;
    case 0b111: // AND
      // rv->X[rd] = rv->X[rs1] & rv->X[rs2];
      gen_and_eax_ecx(block);
      break;
    default:
      assert(!"unreachable");
      break;
    }
    break;
  case 0b0100000:
    switch (funct3) {
    case 0b000: // SUB
      // rv->X[rd] = (int32_t)(rv->X[rs1]) - (int32_t)(rv->X[rs2]);
      gen_sub_eax_ecx(block);
      break;
    case 0b101: // SRA
      // rv->X[rd] = ((int32_t)rv->X[rs1]) >> (rv->X[rs2] & 0x1f);
      gen_and_cl_imm8(block, 0x1f);
      gen_sar_eax_cl(block);
      break;
    default:
      assert(!"unreachable");
      break;
    }
    break;

#if RISCV_VM_SUPPORT_RV32M
  case 0b0000001:
    // RV32M instructions
    switch (funct3) {
    case 0b000: // MUL
      gen_imul_ecx(block);
      break;
    case 0b001: // MULH
      gen_imul_ecx(block);
      gen_mov_eax_edx(block);
      break;
    case 0b010: // MULHSU
      // const int64_t a = (int32_t)rv->X[rs1];
      // const uint64_t b = rv->X[rs2];
      // rv->X[rd] = ((uint64_t)(a * b)) >> 32;
      break;
    case 0b011: // MULHU
      gen_mul_ecx(block);
      gen_mov_eax_edx(block);
      break;
    case 0b100: // DIV
    {
      // const int32_t dividend = (int32_t)rv->X[rs1];
      // const int32_t divisor = (int32_t)rv->X[rs2];
      // if (divisor == 0) {
      //   rv->X[rd] = ~0u;
      // }
      // else if (divisor == -1 && rv->X[rs1] == 0x80000000u) {
      //   rv->X[rd] = rv->X[rs1];
      // }
      // else {
      //   rv->X[rd] = dividend / divisor;
      // }
    }
    break;
    case 0b101: // DIVU
    {
      // const uint32_t dividend = rv->X[rs1];
      // const uint32_t divisor = rv->X[rs2];
      // if (divisor == 0) {
      //   rv->X[rd] = ~0u;
      // }
      // else {
      //   rv->X[rd] = dividend / divisor;
      // }
    }
    break;
    case 0b110: // REM
    {
      // const int32_t dividend = rv->X[rs1];
      // const int32_t divisor = rv->X[rs2];
      // if (divisor == 0) {
      //   rv->X[rd] = dividend;
      // }
      // else if (divisor == -1 && rv->X[rs1] == 0x80000000u) {
      //   rv->X[rd] = 0;
      // }
      // else {
      //   rv->X[rd] = dividend % divisor;
      // }
    }
    break;
    case 0b111: // REMU
    {
      // const uint32_t dividend = rv->X[rs1];
      // const uint32_t divisor = rv->X[rs2];
      // if (divisor == 0) {
      //   rv->X[rd] = dividend;
      // }
      // else {
      //   rv->X[rd] = dividend % divisor;
      // }
    }
    break;
    default:
      assert(!"unreachable");
      break;
    }
    break;
#endif  // RISCV_VM_SUPPORT_RV32M
  default:
    assert(!"unreachable");
    break;
  }

  // rv->X[rd] = rax
  gen_mov_rv32reg_eax(block, rv, rd);
  // step over instruction
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_lui(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t val = dec_utype_imm(inst);
  // rv->X[rd] = val;
  if (rd != rv_reg_zero) {
    gen_mov_eax_imm32(block, val);
    gen_mov_rv32reg_eax(block, rv, rd);
  }
  // step over instruction
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_branch(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // the effective current PC
  const uint32_t pc = block->pc_end;
  // b-type decode
  const uint32_t func3 = dec_funct3(inst);
  const int32_t  imm   = dec_btype_imm(inst);
  const uint32_t rs1   = dec_rs1(inst);
  const uint32_t rs2   = dec_rs2(inst);
  // perform the compare
  gen_mov_eax_rv32reg(block, rv, rs1);
  gen_mov_ecx_rv32reg(block, rv, rs2);
  gen_cmp_eax_ecx(block);
  // load both targets
  gen_mov_eax_imm32(block, pc + 4);
  gen_mov_edx_imm32(block, pc + imm);
  // dispatch by branch type
  switch (func3) {
  case 0: // BEQ
    // taken = (rv->X[rs1] == rv->X[rs2]);
    gen_cmove_eax_edx(block);
    break;
  case 1: // BNE
    // taken = (rv->X[rs1] != rv->X[rs2]);
    gen_cmovne_eax_edx(block);
    break;
  case 4: // BLT
    // taken = ((int32_t)rv->X[rs1] < (int32_t)rv->X[rs2]);
    gen_cmovl_eax_edx(block);
    break;
  case 5: // BGE
    // taken = ((int32_t)rv->X[rs1] >= (int32_t)rv->X[rs2]);
    gen_cmovge_eax_edx(block);
    break;
  case 6: // BLTU
    // taken = (rv->X[rs1] < rv->X[rs2]);
    gen_cmovb_eax_edx(block);
    break;
  case 7: // BGEU
    // taken = (rv->X[rs1] >= rv->X[rs2]);
    gen_cmovnb_eax_edx(block);
    break;
  default:
    assert(!"unreachable");
  }
  // load PC with the target
  gen_mov_rv32pc_eax(block, rv);
  // step over instruction
  block->pc_end += 4;
  // could branch
  return false;
}

static bool op_jalr(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // the effective current PC
  const uint32_t pc = block->pc_end;
  // i-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t  imm = dec_itype_imm(inst);

  // jump
  // note: we also clear the least significant bit of pc
  gen_mov_eax_rv32reg(block, rv, rs1);
  gen_add_eax_imm32(block, imm);
  gen_and_eax_imm32(block, 0xfffffffe);
  gen_mov_rv32pc_eax(block, rv);

  // link
  if (rd != rv_reg_zero) {
    const uint32_t ret_addr = pc + 4;
    gen_mov_eax_imm32(block, ret_addr);
    gen_mov_rv32reg_eax(block, rv, rd);
  }

  // check for exception
  // if (rv->PC & 0x3) {
  //   raise_exception(rv, rv_except_inst_misaligned);
  // }

  // step over instruction
  block->pc_end += 4;
  // could branch
  return false;
}

static bool op_jal(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // the effective current PC
  const uint32_t pc = block->pc_end;
  // j-type decode
  const uint32_t rd  = dec_rd(inst);
  const int32_t rel = dec_jtype_imm(inst);

  // jump
  // note: rel is aligned to a two byte boundary so we dont needs to do any
  //       masking here.
  gen_mov_eax_imm32(block, pc + rel);
  gen_mov_rv32pc_eax(block, rv);

  // link
  if (rd != rv_reg_zero) {
    const uint32_t ret_addr = pc + 4;
    gen_mov_eax_imm32(block, ret_addr);
    gen_mov_rv32reg_eax(block, rv, rd);
  }

  // check alignment of PC
  // if (rv->PC & 0x3) {
  //   raise_exception(rv, rv_except_inst_misaligned);
  // }

  // step over instruction
  block->pc_end += 4;
  // could branch
  return false;
}

static bool op_system(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // the effective current PC
  const uint32_t pc = block->pc_end;
  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const int32_t  csr    = dec_csr(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rd     = dec_rd(inst);

  // arg1
  gen_mov_rcx_imm64(block, (uint64_t)rv);
  // arg2
  gen_mov_edx_imm32(block, pc);
  // arg3
  gen_mov_r8_imm32(block, inst);

  // dispatch by func3 field
  switch (funct3) {
  case 0:
    // dispatch from imm field
    switch (imm) {
    case 0: // ECALL
      // rv->io.on_ecall(rv, rv->PC, inst);
      gen_mov_r9_imm64(block, (uint64_t)rv->io.on_ecall);
      gen_call_r9(block);
      break;
    case 1: // EBREAK
      // rv->io.on_ebreak(rv, rv->PC, inst);
      gen_mov_r9_imm64(block, (uint64_t)rv->io.on_ebreak);
      gen_call_r9(block);
      break;
    default:
      assert(!"unreachable");
    }
    break;
  case 1:
  case 2:
  case 3:
    // TODO: CSRRW, CSRRS, CSRRC
    break;
  default:
    assert(!"unreachable");
  }

  // step over to next instruction
  // XXX: this effectively stops ecall or ebreak changing PC
  gen_mov_eax_imm32(block, pc + 4);
  gen_mov_rv32pc_eax(block, rv);

  // step over instruction
  block->pc_end += 4;
  // could branch
  return false;
}

// opcode handler type
typedef bool(*opcode_t)(struct riscv_t *rv, uint32_t inst, struct block_t *block);

// opcode dispatch table
static const opcode_t opcodes[] = {
//  000        001          010       011          100        101       110   111
    op_load,   NULL,        NULL,     NULL,        op_op_imm, op_auipc, NULL, NULL, // 00
    op_store,  NULL,        NULL,     NULL,        op_op,     op_lui,   NULL, NULL, // 01
    NULL,      NULL,        NULL,     NULL,        NULL,      NULL,     NULL, NULL, // 10
    op_branch, op_jalr,     NULL,     op_jal,      op_system, NULL,     NULL, NULL, // 11
};

static void rv_translate_block(struct riscv_t *rv, struct block_t *block) {
  assert(rv);

  // setup the basic block
  block->instructions = 0;
  block->pc_start = rv->PC;
  block->pc_end = rv->PC;
  block->head = 0;

  for (;;) {
    JITPRINTF("// %08xh\n", block->pc_end);
    // fetch the next instruction
    const uint32_t inst = rv->io.mem_ifetch(rv, block->pc_end);
    const uint32_t index = (inst & INST_6_2) >> 2;
    // find translation function
    const opcode_t op = opcodes[index];
    if (!op) {
      break;
    }
    // translate this instructions
    block->instructions += 1;
    if (!op(rv, inst, block)) {
      break;
    }
  }

  // finalize the basic block
  gen_ret(block);
}

uint32_t rv_step_jit(struct riscv_t *rv) {

  // lookup a block for this PC
  struct block_t *block = block_find(&rv->jit, rv->PC);
  if (!block) {
    block = block_alloc(&rv->jit);
    assert(block);
    rv_translate_block(rv, block);
    block_finish(&rv->jit, block);
  }

  // we should have a block by now
  assert(block);

  // call the translated block
  typedef void(*call_block_t)(void);
  call_block_t c = (call_block_t)block->code;
  c();

  // return number of instructions executed
  return block->instructions;
}

bool rv_init_jit(struct riscv_t *rv) {
  static const uint32_t code_size = 1024 * 1024 * 4;
  static const uint32_t map_size = 1024 * 64;

  struct riscv_jit_t *jit = &rv->jit;

  // allocate the block map which maps address to blocks
  if (jit->block_map == NULL) {
    jit->block_map_size = map_size;
    jit->block_map = malloc(map_size * sizeof(struct block_t*));
    memset(jit->block_map, 0, map_size * sizeof(struct block_t*));
  }

  // allocate block/code storage space
  if (jit->start == NULL) {
    void *ptr = VirtualAlloc(NULL, code_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memset(ptr, 0xcc, code_size);
    jit->start = ptr;
    jit->end = jit->start + code_size;
    jit->head = ptr;
  }

  return true;
}
