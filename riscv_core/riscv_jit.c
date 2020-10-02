#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "riscv.h"
#include "riscv_private.h"


enum {
  x64_rax,  // volatile       
  x64_rbx,  // save           
  x64_rcx,  // volatile       arg1
  x64_rdx,  // volatile       arg2
  x64_rdi,  // save           
  x64_rsi,  // save           
  x64_rbp,  // save           
  x64_rsp,  // save           
  x64_r8,   // volatile       arg3
  x64_r9,   // volatile       arg4
  x64_r10,  // volatile       
  x64_r11,  // volatile       
  x64_r12,  // save           
  x64_r13,  // save           
  x64_r14,  // save           
  x64_r15,  // save           
};

struct block_t {
  uint32_t instructions;
  uint32_t pc_start;
  uint32_t pc_end;
};

static const char *reg_name_x64(uint32_t reg) {
  const char *name[] = {
    "rax", "rbx", "rcx", "rdx", "rdi", "rsi", "rbp", "rsp", "r8 ", "r9 ",
    "r10", "r11", "r12", "r13", "r14", "r15",
  };
  return name[reg];
}

static void gen_get_reg(struct riscv_t *rv, uint32_t x64_reg, uint32_t riscv_reg) {
  if (riscv_reg == rv_reg_zero) {
    printf("xor %s, %s\n", reg_name_x64(x64_reg), reg_name_x64(x64_reg));
  }
  else {
    printf("mov %s, rv->X[%u]\n", reg_name_x64(x64_reg), riscv_reg);
  }
}

static void gen_get_imm(struct riscv_t *rv, uint32_t x64_reg, uint32_t imm) {
  printf("mov %s, %u\n", reg_name_x64(x64_reg), imm);
}

static void gen_set_reg(struct riscv_t *rv, uint32_t riscv_reg, uint32_t x64_reg) {
  if (riscv_reg != rv_reg_zero) {
    printf("mov rv->X[%u], %s\n", riscv_reg, reg_name_x64(x64_reg));
  }
}

static void gen_get_pc(struct riscv_t *rv, uint32_t x64_reg) {
  printf("mov %s, rv->PC\n", reg_name_x64(x64_reg));
}

static void gen_set_pc(struct riscv_t *rv, uint32_t x64_reg) {
  printf("mov rv->PC, %s\n", reg_name_x64(x64_reg));
}

static void gen_add_reg_imm(uint32_t x64_dst, int32_t imm) {
  printf("add %s, %d\n", reg_name_x64(x64_dst), imm);
}

static void gen_add_reg_immu(uint32_t x64_dst, uint32_t imm) {
  printf("add %s, %d\n", reg_name_x64(x64_dst), imm);
}

static void gen_set_reg_immu(struct riscv_t *rv, uint32_t x64_dst, uint32_t imm) {
  printf("mov %s, %u\n", reg_name_x64(x64_dst), imm);
}

static void gen_emit_byte(uint8_t op) {
  //
}

static void gen_emit_data(uint8_t *ptr, uint32_t size) {
  //
}

static void gen_mov_rax_imm(uint32_t imm) {
  gen_emit_byte(0x48);
  gen_emit_byte(0xc7);
  gen_emit_byte(0xc0);
  gen_emit_data(&imm, 4);
}

static bool op_load(struct riscv_t *rv, uint32_t inst, struct block_t *block) {
  // itype format
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rd     = dec_rd(inst);
  // load address
  // rdx = rv->X[rs1] + imm;
  gen_get_reg(rv, x64_rdx, rs1);
  gen_add_reg_imm(x64_rdx, imm);
  // dispatch by read size
  switch (funct3) {
  case 0: // LB
    // rv->X[rd] = sign_extend_b(rv->io.mem_read_b(rv, addr));
    {
      printf("mov rcx, rv");
      printf("call rv->io.mem_read_b");
      printf("movsx rax, al");
    }
    break;
  case 1: // LH
    // rv->X[rd] = sign_extend_h(rv->io.mem_read_s(rv, addr));
    {
      printf("mov rcx, rv");
      printf("call rv->io.mem_read_s");
      printf("movsx rax, ax");
    }
    break;
  case 2: // LW
    // rv->X[rd] = rv->io.mem_read_w(rv, addr);
    {
      printf("mov rcx, rv");
      printf("call rv->io.mem_read_w");
    }
    break;
  case 4: // LBU
    // rv->X[rd] = rv->io.mem_read_b(rv, addr);
    {
      printf("mov rcx, rv");
      printf("call rv->io.mem_read_b");
    }
    break;
  case 5: // LHU
    // rv->X[rd] = rv->io.mem_read_s(rv, addr);
    {
      printf("mov rcx, rv");
      printf("call rv->io.mem_read_s");
    }
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = rax
  if (rd != rv_reg_zero) {
    gen_set_reg(rv, rd, x64_rax);
  }
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
  // rax = rv->X[rs1]
  gen_get_reg(rv, x64_rax, rs1);
  // dispatch operation type
  switch (funct3) {
  case 0: // ADDI
    // rv->X[rd] = (int32_t)(rv->X[rs1]) + imm;
    printf("add rax, %d\n", imm);
    break;
  case 1: // SLLI
    // rv->X[rd] = rv->X[rs1] << (imm & 0x1f);
    printf("shl rax, %d\n", (imm & 0x1f));
    break;
  case 2: // SLTI
    // rv->X[rd] = ((int32_t)(rv->X[rs1]) < imm) ? 1 : 0;
    printf("cmp rax, imm\n");
    printf("xor rax, rax\n");
    printf("setl rax\n"); // signed
    break;
  case 3: // SLTIU
    // rv->X[rd] = (rv->X[rs1] < (uint32_t)imm) ? 1 : 0;
    printf("cmp rax, imm\n");
    printf("xor rax, rax\n");
    printf("setb rax\n"); // unsigned
    break;
  case 4: // XORI
    // rv->X[rd] = rv->X[rs1] ^ imm;
    printf("xor rax, %08xh\n", imm);
    break;
  case 5:
    if (imm & ~0x1f) {
      // SRAI
      // rv->X[rd] = ((int32_t)rv->X[rs1]) >> (imm & 0x1f);
      printf("sar rax, %d\n", (imm & 0x1f));
    }
    else {
      // SRLI
      // rv->X[rd] = rv->X[rs1] >> (imm & 0x1f);
      printf("shr rax, %d\n", (imm & 0x1f));
    }
    break;
  case 6: // ORI
    // rv->X[rd] = rv->X[rs1] | imm;
    printf("or rax, %08xh\n", imm);
    break;
  case 7: // ANDI
    // rv->X[rd] = rv->X[rs1] & imm;
    printf("and rax, %08xh\n", imm);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = rax
  if (rd != rv_reg_zero) {
    gen_set_reg(rv, rd, x64_rax);
  }
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
  // rv->X[rd] = imm + rv->PC;
  gen_get_pc(rv, x64_rax);
  gen_add_reg_immu(x64_rax, imm);
  if (rd != rv_reg_zero) {
    gen_set_reg(rv, rd, x64_rax);
  }
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
  // store address
  printf("mov rax, rv\n");
  // const uint32_t addr = rv->X[rs1] + imm;
  gen_get_reg(rv, x64_rdx, rs1);
  gen_add_reg_imm(x64_rdx, imm);
  // const uint32_t data = rv->X[rs2];
  gen_get_reg(rv, x64_r8, rs2);
  // dispatch by write size
  switch (funct3) {
  case 0: // SB
    // rv->io.mem_write_b(rv, addr, data);
    printf("call rv->io.mem_write_b\n");
    break;
  case 1: // SH
    // rv->io.mem_write_s(rv, addr, data);
    printf("call rv->io.mem_write_s\n");
    break;
  case 2: // SW
    // rv->io.mem_write_w(rv, addr, data);
    printf("call rv->io.mem_write_w\n");
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

  // rs1
  gen_get_reg(rv, x64_rax, rs1);
  // rs2
  gen_get_reg(rv, x64_rcx, rs2);

  switch (funct7) {
  case 0b0000000:
    switch (funct3) {
    case 0b000: // ADD
      // rv->X[rd] = (int32_t)(rv->X[rs1]) + (int32_t)(rv->X[rs2]);
      printf("add rax, rcx\n");
      gen_emit_data("\x48\x21\xC8", 3);  // and rax, rcx
      break;
    case 0b001: // SLL
      // rv->X[rd] = rv->X[rs1] << (rv->X[rs2] & 0x1f);
      printf("and cx, 0x1f\n");
      printf("shl rax, cx\n");
      break;
    case 0b010: // SLT
      // rv->X[rd] = ((int32_t)(rv->X[rs1]) < (int32_t)(rv->X[rs2])) ? 1 : 0;
      printf("cmp rax, rcx\n");
      printf("xor rax, rax\n");
      printf("setl ax\n"); // signed
      break;
    case 0b011: // SLTU
      // rv->X[rd] = (rv->X[rs1] < rv->X[rs2]) ? 1 : 0;
      printf("cmp rax, rcx\n");
      printf("xor rax, rax\n");
      printf("setb ax\n"); // unsigned
      break;
    case 0b100: // XOR
      // rv->X[rd] = rv->X[rs1] ^ rv->X[rs2];
      printf("xor rax, rcx\n");
      gen_emit_data("\x48\x31\xC8", 3); // xor rax, rcx
      break;
    case 0b101: // SRL
      // rv->X[rd] = rv->X[rs1] >> (rv->X[rs2] & 0x1f);
      printf("and cx, 0x1f\n");
      printf("shr rax, cx\n");
      break;
    case 0b110: // OR
      // rv->X[rd] = rv->X[rs1] | rv->X[rs2];
      printf("or rax, rcx\n");
      gen_emit_data("\x48\x09\xC8", 3); // or rax, rcx
      break;
    case 0b111: // AND
      // rv->X[rd] = rv->X[rs1] & rv->X[rs2];
      printf("and rax, rcx\n");
      gen_emit_data("\x48\x21\xC8", 3); // and rax, rcx
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
      printf("sub rax, rcx\n");
      gen_emit_data("\x48\x29\xC8", 3);  // sub rax, rcx
      break;
    case 0b101: // SRA
      // rv->X[rd] = ((int32_t)rv->X[rs1]) >> (rv->X[rs2] & 0x1f);
      printf("and cx, 0x1f");
      printf("sar rax, cx\n");
      gen_emit_data("\x66\x83\xE1\x1F", 4);   // and cx, 0x1f
      gen_emit_data("\x48\xD3\xF8", 3);       // sar rax, cl
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
  // rv->X[rd] = rax
  if (rd != rv_reg_zero) {
    gen_set_reg(rv, rd, x64_rax);
  }
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
    printf("mov rax, %02xh\n", val);
    gen_set_reg(rv, rd, x64_rax);
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
  // jump target
  const uint32_t target = pc + imm;
  // r8 = pc + 4
  gen_set_reg_immu(rv, x64_r8, pc + 4);
  // rs1
  gen_get_reg(rv, x64_rax, rs1);
  // rs2
  gen_get_reg(rv, x64_rcx, rs2);
  // compare
  printf("cmp rax, rcx\n");
  gen_emit_data("\x48\x39\xC8", 3);   // cmp rax, rcx
  // dispatch by branch type
  switch (func3) {
  case 0: // BEQ
    // taken = (rv->X[rs1] == rv->X[rs2]);
    printf("cmove r8, %02xh\n", target);
    break;
  case 1: // BNE
    // taken = (rv->X[rs1] != rv->X[rs2]);
    printf("cmovne r8, %02xh\n", target);
    break;
  case 4: // BLT
    // taken = ((int32_t)rv->X[rs1] < (int32_t)rv->X[rs2]);
    printf("cmovlt r8, %02xh\n", target);
    break;
  case 5: // BGE
    // taken = ((int32_t)rv->X[rs1] >= (int32_t)rv->X[rs2]);
    printf("cmovge r8, %02xh\n", target);
    break;
  case 6: // BLTU
    // taken = (rv->X[rs1] < rv->X[rs2]);
    printf("cmovb r8, %02xh\n", target);
    break;
  case 7: // BGEU
    // taken = (rv->X[rs1] >= rv->X[rs2]);
    printf("cmovnb r8, %02xh\n", target);
    break;
  default:
    assert(!"unreachable");
  }
  // load PC with the target
  gen_set_pc(rv, x64_r8);
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

  // compute return address
  // const uint32_t ra = rv->PC + 4;

  // jump
  // rv->PC = (rv->X[rs1] + imm) & ~1u;
  gen_get_reg(rv, x64_rax, rs1);
  gen_add_reg_imm(x64_rax, imm);
  printf("and al, 0xfe");
  gen_emit_data("\x24\xfe", 2);  // and al, 0xfe
  gen_set_pc(rv, x64_rax);

  // link
  // if (rd != rv_reg_zero) {
  //   rv->X[rd] = ra;
  // }

  if (rd != rv_reg_zero) {
    const uint32_t ret_addr = pc + 4;
    printf("mov rax, %02xh\n", ret_addr);
    gen_set_reg(rv, rd, x64_rax);
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

  // compute return address
  // const uint32_t ra = rv->PC + 4;

  // jump
  // rv->PC += rel;
  gen_get_pc(rv, x64_rax);
  gen_add_reg_imm(rel, rel);
  gen_set_pc(rv, x64_rax);

  // link
  // if (rd != rv_reg_zero) {
  //   rv->X[rd] = ra;
  // }

  if (rd != rv_reg_zero) {
    const uint32_t ret_addr = pc + 4;
    printf("mov rax, %02xh\n", ret_addr);
    gen_set_reg(rv, rd, x64_rax);
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

  printf("mov rcx, rv\n");
  printf("mov rdx, %02xh\n", pc);
  printf("mov r8, %02xh\n", inst);

  // dispatch by func3 field
  switch (funct3) {
  case 0:
    // dispatch from imm field
    switch (imm) {
    case 0: // ECALL
      // rv->io.on_ecall(rv, rv->PC, inst);
      printf("call rv->io.on_ecall\n");
      break;
    case 1: // EBREAK
      // rv->io.on_ebreak(rv, rv->PC, inst);
      printf("call rv->io.on_ebreak\n");
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

  while (true) {

    printf("// %08xh\n", block->pc_end);

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

    // XXX: for testing
    break;
  }

  // finalize the basic block
  printf("ret\n");
  gen_emit_byte("\c3");  // ret
}

bool rv_step_jit(struct riscv_t *rv) {

  struct block_t block;
  rv_translate_block(rv, &block);

  return false;
}
