#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../riscv_emu/riscv.h"
#include "../riscv_emu/riscv_private.h"

#include "riscv_jit_2.h"


static bool op_load(struct riscv_t *rv, uint32_t inst, struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // itype format
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rd     = dec_rd(inst);

  // skip writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    builder->pc += 4;
    builder->instructions += 1;
    return true;
  }

  // generate load address
  struct ir_inst_t *ir_addr = ir_add(z, ir_ld_reg(z, rs1), ir_imm(z, imm));
  struct ir_inst_t *ir_load = NULL;

  // dispatch by read size
  switch (funct3) {
  case 0: // LB
    ir_load = ir_lb(z, ir_addr);
    break;
  case 1: // LH
    ir_load = ir_lh(z, ir_addr);
    break;
  case 2: // LW
    ir_load = ir_lw(z, ir_addr);
    break;
  case 4: // LBU
    ir_load = ir_lbu(z, ir_addr);
    break;
  case 5: // LHU
    ir_load = ir_lhu(z, ir_addr);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = rax
  ir_st_reg(z, rd, ir_load);
  // step over instruction
  builder->pc += 4;
  builder->instructions += 1;
  // cant branch
  return true;
}

static bool op_op_imm(struct riscv_t *rv,
                      uint32_t inst,
                      struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    builder->pc += 4;
    builder->instructions += 1;
    return true;
  }

  struct ir_inst_t *lhs = ir_ld_reg(z, rs1);
  struct ir_inst_t *res = NULL;

  // dispatch operation type
  switch (funct3) {
  case 0: // ADDI
    res = ir_add(z, lhs, ir_imm(z, imm));
    break;
  case 1: // SLLI
    res = ir_shl(z, lhs, ir_imm(z, imm & 0x1f));
    break;
  case 2: // SLTI
    res = ir_lt(z, lhs, ir_imm(z, imm));
    break;
  case 3: // SLTIU
    res = ir_ltu(z, lhs, ir_imm(z, imm));
    break;
  case 4: // XORI
    res = ir_xor(z, lhs, ir_imm(z, imm));
    break;
  case 5:
    if (imm & ~0x1f) {
      // SRAI
      res = ir_sar(z, lhs, ir_imm(z, imm & 0x1f));
    }
    else {
      // SRLI
      res = ir_shr(z, lhs, ir_imm(z, imm & 0x1f));
    }
    break;
  case 6: // ORI
    res = ir_or(z, lhs, ir_imm(z, imm));
    break;
  case 7: // ANDI
    res = ir_and(z, lhs, ir_imm(z, imm));
    break;
  default:
    assert(!"unreachable");
    break;
  }

  ir_st_reg(z, rd, res);
  // step over instruction
  builder->pc += 4;
  builder->instructions += 1;
  // cant branch
  return true;
}

// add upper immediate to pc
static bool op_auipc(struct riscv_t *rv,
                     uint32_t inst,
                     struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // the effective current PC
  const uint32_t pc = builder->pc;
  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t imm = dec_utype_imm(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    builder->pc += 4;
    builder->instructions += 1;
    return true;
  }

  // rv->X[rd] = imm + rv->PC;
  ir_st_reg(z, rd, ir_imm(z, pc + imm));

  // step over instruction
  builder->pc += 4;
  builder->instructions += 1;
  // cant branch
  return true;
}

static bool op_store(struct riscv_t *rv,
                     uint32_t inst,
                     struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // s-type format
  const int32_t  imm    = dec_stype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct3 = dec_funct3(inst);

  // generate store address
  struct ir_inst_t *ir_addr = ir_add(z, ir_ld_reg(z, rs1), ir_imm(z, imm));

  // dispatch by write size
  switch (funct3) {
  case 0: // SB
    ir_sb(z, ir_addr, ir_ld_reg(z, rs2));
    break;
  case 1: // SH
    ir_sh(z, ir_addr, ir_ld_reg(z, rs2));
    break;
  case 2: // SW
    ir_sw(z, ir_addr, ir_ld_reg(z, rs2));
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // step over instruction
  builder->pc += 4;
  builder->instructions += 1;
  // cant branch
  return true;
}

static bool op_op(struct riscv_t *rv,
                  uint32_t inst,
                  struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // effective pc
  const uint32_t pc = builder->pc;
  // r-type decode
  const uint32_t rd     = dec_rd(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct7 = dec_funct7(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    builder->pc += 4;
    builder->instructions += 1;
    return true;
  }

  struct ir_inst_t *lhs = ir_ld_reg(z, rs1);
  struct ir_inst_t *rhs = ir_ld_reg(z, rs2);
  struct ir_inst_t *res = NULL;

  switch (funct7) {
  case 0b0000000:
    switch (funct3) {
    case 0b000: // ADD
      res = ir_add(z, lhs, rhs);
      break;
    case 0b001: // SLL
      res = ir_shl(z, lhs, ir_and(z, rhs, ir_imm(z, 0x1f)));
      break;
    case 0b010: // SLT
      res = ir_lt(z, lhs, rhs);
      break;
    case 0b011: // SLTU
      res = ir_ltu(z, lhs, rhs);
      break;
    case 0b100: // XOR
      res = ir_xor(z, lhs, rhs);
      break;
    case 0b101: // SRL
      res = ir_shr(z, lhs, ir_and(z, rhs, ir_imm(z, 0x1f)));
      break;
    case 0b110: // OR
      res = ir_or(z, lhs, rhs);
      break;
    case 0b111: // AND
      res = ir_and(z, lhs, rhs);
      break;
    default:
      assert(!"unreachable");
      break;
    }
    break;
  case 0b0100000:
    switch (funct3) {
    case 0b000: // SUB
      res = ir_sub(z, lhs, rhs);
      break;
    case 0b101: // SRA
      res = ir_sar(z, lhs, ir_and(z, rhs, ir_imm(z, 0x1f)));
      break;
    default:
      assert(!"unreachable");
      break;
    }
    break;
  default:
    assert(!"unreachable");
    break;
  }

  ir_st_reg(z, rd, res);
  // step over instruction
  builder->instructions += 1;
  builder->pc += 4;
  // cant branch
  return true;
}

static bool op_lui(struct riscv_t *rv,
                   uint32_t inst,
                   struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t val = dec_utype_imm(inst);
  // rv->X[rd] = val;
  if (rd != rv_reg_zero) {
    ir_st_reg(z, rd, ir_imm(z, val));
  }
  // step over instruction
  builder->instructions += 1;
  builder->pc += 4;
  // cant branch
  return true;
}

static bool op_branch(struct riscv_t *rv,
                      uint32_t inst,
                      struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // the effective current PC
  const uint32_t pc = builder->pc;
  // b-type decode
  const uint32_t func3 = dec_funct3(inst);
  const int32_t  imm   = dec_btype_imm(inst);
  const uint32_t rs1   = dec_rs1(inst);
  const uint32_t rs2   = dec_rs2(inst);

  struct ir_inst_t *lhs = ir_ld_reg(z, rs1);
  struct ir_inst_t *rhs = ir_ld_reg(z, rs2);
  struct ir_inst_t *next = ir_imm(z, pc + 4);
  struct ir_inst_t *targ = ir_imm(z, pc + imm);
  struct ir_inst_t *cond = NULL;

  // dispatch by branch type
  switch (func3) {
  case 0: // BEQ
    cond = ir_eq(z, lhs, rhs);
    break;
  case 1: // BNE
    cond = ir_neq(z, lhs, rhs);
    break;
  case 4: // BLT
    cond = ir_lt(z, lhs, rhs);
    break;
  case 5: // BGE
    cond = ir_ge(z, lhs, rhs);
    break;
  case 6: // BLTU
    cond = ir_ltu(z, lhs, rhs);
    break;
  case 7: // BGEU
    cond = ir_geu(z, lhs, rhs);
    break;
  default:
    assert(!"unreachable");
  }
  // load PC with the target
  ir_branch(z, cond, targ, next);
  // step over instruction
  builder->instructions += 1;
  builder->pc += 4;
  // could branch
  return false;
}

static bool op_jalr(struct riscv_t *rv,
                    uint32_t inst,
                    struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // the effective current PC
  const uint32_t pc = builder->pc;
  // i-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t  imm = dec_itype_imm(inst);

  // jump
  // note: we also clear the least significant bit of pc

  struct ir_inst_t *addr =
    ir_and(z, ir_add(z, ir_ld_reg(z, rs1), ir_imm(z, imm)), ir_imm(z, 0xfffffffe));
  ir_st_pc(z, addr);

  // link
  if (rd != rv_reg_zero) {
    ir_st_reg(z, rd, ir_imm(z, pc + 4));
  }

  // step over instruction
  builder->instructions += 1;
  builder->pc += 4;
  // could branch
  return false;
}

static bool op_jal(struct riscv_t *rv,
                   uint32_t inst,
                   struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // the effective current PC
  const uint32_t pc = builder->pc;
  // j-type decode
  const uint32_t rd  = dec_rd(inst);
  const int32_t rel = dec_jtype_imm(inst);

  // jump
  // note: rel is aligned to a two byte boundary so we dont needs to do any
  //       masking here.

  ir_st_pc(z, ir_imm(z, pc + rel));

  // link
  if (rd != rv_reg_zero) {
    ir_st_reg(z, rd, ir_imm(z, pc + 4));
  }

  // step over instruction
  builder->instructions += 1;
  builder->pc += 4;
  // could branch
  return false;
}

static bool op_system(struct riscv_t *rv,
                      uint32_t inst,
                      struct ir_builder_t *builder) {
  struct ir_block_t *z = &builder->ir;

  // the effective current PC
  const uint32_t pc = builder->pc;
  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const int32_t  csr    = dec_csr(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rd     = dec_rd(inst);

  // update PC prior to calls
  ir_st_pc(z, ir_imm(z, pc + 4));

  // dispatch by func3 field
  switch (funct3) {
  case 0:
    // dispatch from imm field
    switch (imm) {
    case 0: // ECALL
      ir_ecall(z);
      break;
    case 1: // EBREAK
      ir_ebreak(z);
      break;
    default:
      assert(!"unreachable");
    }
    break;
  default:
    // treat unknown as a nop for now
    break;
  }

  // step over instruction
  builder->instructions += 1;
  builder->pc += 4;

  return false;
}

// opcode handler type
typedef bool(*opcode_t)(struct riscv_t *rv,
                        uint32_t inst,
                        struct ir_builder_t *builder);

// opcode dispatch table
static const opcode_t opcodes[] = {
//  000        001          010       011          100        101       110   111
    op_load,   NULL,        NULL,     NULL,        op_op_imm, op_auipc, NULL, NULL, // 00
    op_store,  NULL,        NULL,     NULL,        op_op,     op_lui,   NULL, NULL, // 01
    NULL,      NULL,        NULL,     NULL,        NULL,      NULL,     NULL, NULL, // 10
    op_branch, op_jalr,     NULL,     op_jal,      op_system, NULL,     NULL, NULL, // 11
};

static int32_t rv_block_eval(struct riscv_t *rv) {
  assert(rv);

  struct ir_builder_t builder;
  ir_init(&builder.ir);
  builder.pc = rv->PC;
  builder.instructions = 0;

  struct ir_block_t *z = &builder.ir;

  // translate the basic block
  for (;;) {
    // fetch the next instruction
    const uint32_t inst = rv->io.mem_ifetch(rv, builder.pc);
    const uint32_t index = (inst & INST_6_2) >> 2;
    // find translation function
    const opcode_t op = opcodes[index];
    if (!op) {
      // we dont have a handler for this instruction so end basic block
      // make sure PC gets updated
      ir_st_pc(z, ir_imm(z, builder.pc));
      break;
    }
    if (!op(rv, inst, &builder)) {
      break;
    }
  }

  // evaluate the IR
  ir_eval(z, rv);

  return builder.instructions;
}

bool rv_step_jit(struct riscv_t *rv, const uint64_t cycles_target) {
  int32_t retired = rv_block_eval(rv);
  return retired > 0;
}

bool rv_init_jit(struct riscv_t *rv) {
  return true;
}
