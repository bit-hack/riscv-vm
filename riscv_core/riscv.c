#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "riscv.h"


// enable experimental RV32M support
#define SUPPORT_RV32M 1

#define RV_NUM_REGS 32


// csrs
enum {
  // floating point
  csr_fflags    = 0x001,
  csr_frm       = 0x002,
  csr_fcsr      = 0x003,
  // low words
  csr_cycle     = 0xC00,
  csr_time      = 0xC01,
  csr_instret   = 0xC02,
  // high words
  csr_cycleh    = 0xC80,
  csr_timeh     = 0xC81,
  csr_instreth  = 0xC82
};

// instruction opcode decode masks
enum {
  //               .xxxxxx....x.....xxx....x.......
  inst_4_2     = 0b00000000000000000000000000011100,
  inst_6_5     = 0b00000000000000000000000001100000,
  //               .xxxxxx....x.....xxx....x.......
};

// instruction type decode masks
enum {
  //               .xxxxxx....x.....xxx....x.......
  fr_opcode    = 0b00000000000000000000000001111111, // r-type
  fr_rd        = 0b00000000000000000000111110000000,
  fr_funct3    = 0b00000000000000000111000000000000,
  fr_rs1       = 0b00000000000011111000000000000000,
  fr_rs2       = 0b00000001111100000000000000000000,
  fr_funct7    = 0b11111110000000000000000000000000,
  //               .xxxxxx....x.....xxx....x.......
  fi_imm_11_0  = 0b11111111111100000000000000000000, // i-type
  //               .xxxxxx....x.....xxx....x.......
  fs_imm_4_0   = 0b00000000000000000000111110000000, // s-type
  fs_imm_11_5  = 0b11111110000000000000000000000000,
  //               .xxxxxx....x.....xxx....x.......
  fb_imm_11    = 0b00000000000000000000000010000000, // b-type
  fb_imm_4_1   = 0b00000000000000000000111100000000,
  fb_imm_10_5  = 0b01111110000000000000000000000000,
  fb_imm_12    = 0b10000000000000000000000000000000,
  //               .xxxxxx....x.....xxx....x.......
  fu_imm_31_12 = 0b11111111111111111111000000000000, // u-type
  //               .xxxxxx....x.....xxx....x.......
  fj_imm_19_12 = 0b00000000000011111111000000000000, // j-type
  fj_imm_11    = 0b00000000000100000000000000000000,
  fj_imm_10_1  = 0b01111111111000000000000000000000,
  fj_imm_20    = 0b10000000000000000000000000000000,
  //               .xxxxxx....x.....xxx....x.......
};

struct riscv_t {
  // io interface
  struct riscv_io_t io;
  // register file
  riscv_word_t X[RV_NUM_REGS];
  riscv_word_t PC;
  // user provided data
  riscv_user_t userdata;
};

// decode rd field
static uint32_t _dec_rd(uint32_t inst) {
  return (inst & fr_rd) >> 7;
}

// decode rs1 field
static uint32_t _dec_rs1(uint32_t inst) {
  return (inst & fr_rs1) >> 15;
}

// decode rs2 field
static uint32_t _dec_rs2(uint32_t inst) {
  return (inst & fr_rs2) >> 20;
}

// decoded funct3 field
static uint32_t _dec_funct3(uint32_t inst) {
  return (inst & fr_funct3) >> 12;
}

// decode funct7 field
static uint32_t _dec_funct7(uint32_t inst) {
  return (inst & fr_funct7) >> 25;
}

// decode utype instruction immediate
static uint32_t _dec_utype_imm(uint32_t inst) {
  return inst & fu_imm_31_12;
}

// decode jtype instruction immediate
static int32_t _dec_jtype_imm(uint32_t inst) {
  uint32_t dst = 0;
  dst |= (inst & fj_imm_20);
  dst |= (inst & fj_imm_19_12) << 11;
  dst |= (inst & fj_imm_11)    << 2;
  dst |= (inst & fj_imm_10_1)  >> 9;
  // note: shifted to 2nd least significant bit
  return ((int32_t)dst) >> 11;
}

// decode itype instruction immediate
static int32_t _dec_itype_imm(uint32_t inst) {
  return ((int32_t)(inst & fi_imm_11_0)) >> 20;
}

static int32_t _dec_btype_imm(uint32_t inst) {
  uint32_t dst = 0;
  dst |= (inst & fb_imm_12);
  dst |= (inst & fb_imm_11) << 23;
  dst |= (inst & fb_imm_10_5) >> 1;
  dst |= (inst & fb_imm_4_1) << 12;
  // note: shifted to 2nd least significant bit
  return ((int32_t)dst) >> 19;
}

static int32_t _dec_stype_imm(uint32_t inst) {
  uint32_t dst = 0;
  dst |= (inst & fs_imm_11_5);
  dst |= (inst & fs_imm_4_0) << 13;
  return ((int32_t)dst) >> 20;
}

// sign extend a 16 bit value
static uint32_t sign_extend_h(uint32_t x) {
  return (int32_t)((int16_t)x);
}

// sign extend an 8 bit value
static uint32_t sign_extend_b(uint32_t x) {
  return (int32_t)((int8_t)x);
}

typedef void(*opcode_t)(struct riscv_t *rv, uint32_t inst);

static void op_load(struct riscv_t *rv, uint32_t inst) {
  // itype format
  const int32_t  imm    = _dec_itype_imm(inst);
  const uint32_t rs1    = _dec_rs1(inst);
  const uint32_t funct3 = _dec_funct3(inst);
  const uint32_t rd     = _dec_rd(inst);
  // load address
  const uint32_t addr = rv->X[rs1] + imm;
  // dispatch by read size
  switch (funct3) {
  case 0: // LB
    rv->X[rd] = sign_extend_b(rv->io.mem_read_b(rv, addr));
    break;
  case 1: // LH
    rv->X[rd] = sign_extend_h(rv->io.mem_read_s(rv, addr));
    break;
  case 2: // LW
    rv->X[rd] = rv->io.mem_read_w(rv, addr);
    break;
  case 4: // LBU
    rv->X[rd] = rv->io.mem_read_b(rv, addr);
    break;
  case 5: // LHU
    rv->X[rd] = rv->io.mem_read_s(rv, addr);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // step over instruction
  rv->PC += 4;
}

static void op_misc_mem(struct riscv_t *rv, uint32_t inst) {
  // FENCE
  rv->PC += 4;
}

static void op_op_imm(struct riscv_t *rv, uint32_t inst) {
  // i-type decode
  const int32_t  imm    = _dec_itype_imm(inst);
  const uint32_t rd     = _dec_rd(inst);
  const uint32_t rs1    = _dec_rs1(inst);
  const uint32_t funct3 = _dec_funct3(inst);
  // dispatch operation type
  switch (funct3) {
  case 0: // ADDI
    rv->X[rd] = (int32_t)(rv->X[rs1]) + imm;
    break;
  case 1: // SLLI
    rv->X[rd] = rv->X[rs1] << (imm & 0x1f);
    break;
  case 2: // SLTI
    rv->X[rd] = ((int32_t)(rv->X[rs1]) < imm) ? 1 : 0;
    break;
  case 3: // SLTIU
    rv->X[rd] = (rv->X[rs1] < (uint32_t)imm) ? 1 : 0;
    break;
  case 4: // XORI
    rv->X[rd] = rv->X[rs1] ^ imm;
    break;
  case 5:
    if (imm & ~0x1f) {
      // SRAI
      rv->X[rd] = ((int32_t)rv->X[rs1]) >> (imm & 0x1f);
    }
    else {
      // SRLI
      rv->X[rd] = rv->X[rs1] >> (imm & 0x1f);
    }
    break;
  case 6: // ORI
    rv->X[rd] = rv->X[rs1] | imm;
    break;
  case 7: // ANDI
    rv->X[rd] = rv->X[rs1] & imm;
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // step over instruction
  rv->PC += 4;
}

// add upper immediate to pc
static void op_auipc(struct riscv_t *rv, uint32_t inst) {
  // u-type decode
  const uint32_t rd  = _dec_rd(inst);
  const uint32_t val = _dec_utype_imm(inst) + rv->PC;
  rv->X[rd] = val;
  // step over instruction
  rv->PC += 4;
}

static void op_store(struct riscv_t *rv, uint32_t inst) {
  // s-type format
  const int32_t  imm    = _dec_stype_imm(inst);
  const uint32_t rs1    = _dec_rs1(inst);
  const uint32_t rs2    = _dec_rs2(inst);
  const uint32_t funct3 = _dec_funct3(inst);
  // store address
  const uint32_t addr = rv->X[rs1] + imm;
  const uint32_t data = rv->X[rs2];
  // dispatch by write size
  switch (funct3) {
  case 0: // SB
    rv->io.mem_write_b(rv, addr, data);
    break;
  case 1: // SH
    rv->io.mem_write_s(rv, addr, data);
    break;
  case 2: // SW
    rv->io.mem_write_w(rv, addr, data);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // step over instruction
  rv->PC += 4;
}

static void op_op(struct riscv_t *rv, uint32_t inst) {
  // r-type decode
  const uint32_t rd     = _dec_rd(inst);
  const uint32_t funct3 = _dec_funct3(inst);
  const uint32_t rs1    = _dec_rs1(inst);
  const uint32_t rs2    = _dec_rs2(inst);
  const uint32_t funct7 = _dec_funct7(inst);

  switch (funct7) {
  case 0b0000000:
    switch (funct3) {
    case 0b000: // ADD
      rv->X[rd] = (int32_t)(rv->X[rs1]) + (int32_t)(rv->X[rs2]);
      break;
    case 0b001: // SLL
      rv->X[rd] = rv->X[rs1] << (rv->X[rs2] & 0x1f);
      break;
    case 0b010: // SLT
      rv->X[rd] = ((int32_t)(rv->X[rs1]) < (int32_t)(rv->X[rs2])) ? 1 : 0;
      break;
    case 0b011: // SLTU
      rv->X[rd] = (rv->X[rs1] < rv->X[rs2]) ? 1 : 0;
      break;
    case 0b100: // XOR
      rv->X[rd] = rv->X[rs1] ^ rv->X[rs2];
      break;
    case 0b101: // SRL
      rv->X[rd] = rv->X[rs1] >> (rv->X[rs2] & 0x1f);
      break;
    case 0b110: // OR
      rv->X[rd] = rv->X[rs1] | rv->X[rs2];
      break;
    case 0b111: // AND
      rv->X[rd] = rv->X[rs1] & rv->X[rs2];
      break;
    default:
      assert(!"unreachable");
      break;
    }
    break;
#if SUPPORT_RV32M
  case 0b0000001:
    // RV32M instructions
    switch (funct3) {
    case 0b000: // MUL
      rv->X[rd] = (int32_t)rv->X[rs1] * (int32_t)rv->X[rs2];
      break;
    case 0b001: // MULH
      {
        const int64_t a = (int32_t)rv->X[rs1];
        const int64_t b = (int32_t)rv->X[rs2];
        rv->X[rd] = ((uint64_t)(a * b)) >> 32;
      }
      break;
    case 0b010: // MULHSU
      {
        const int64_t a = (int32_t)rv->X[rs1];
        const uint64_t b = rv->X[rs2];
        rv->X[rd] = ((uint64_t)(a * b)) >> 32;
      }
      break;
    case 0b011: // MULHU
      rv->X[rd] = ((uint64_t)rv->X[rs1] * (uint64_t)rv->X[rs2]) >> 32;
      break;
    case 0b100: // DIV
      {
        const int32_t dividend = (int32_t)rv->X[rs1];
        const int32_t divisor = (int32_t)rv->X[rs2];
        if (divisor == 0) {
          rv->X[rd] = ~0u;
        }
        else if (divisor == -1 && rv->X[rs1] == 0x80000000u) {
          rv->X[rd] = rv->X[rs1];
        }
        else {
          rv->X[rd] = dividend / divisor;
        }
      }
      break;
    case 0b101: // DIVU
    {
      const uint32_t dividend = rv->X[rs1];
      const uint32_t divisor  = rv->X[rs2];
      if (divisor == 0) {
        rv->X[rd] = ~0u;
      }
      else {
        rv->X[rd] = dividend / divisor;
      }
    }
    break;
      break;
    case 0b110: // REM
      {
        const int32_t dividend = rv->X[rs1];
        const int32_t divisor = rv->X[rs2];
        if (divisor == 0) {
          rv->X[rd] = dividend;
        }
        else if (divisor == -1 && rv->X[rs1] == 0x80000000u) {
          rv->X[rd] = 0;
        }
        else {
          rv->X[rd] = dividend % divisor;
        }
      }
      break;
    case 0b111: // REMU
      {
        const uint32_t dividend = rv->X[rs1];
        const uint32_t divisor = rv->X[rs2];
        if (divisor == 0) {
          rv->X[rd] = dividend;
        }
        else {
          rv->X[rd] = dividend % divisor;
        }
      }
      break;
    default:
      assert(!"unreachable");
      break;
    }
    break;
#endif  // SUPPORT_RV32M
  case 0b0100000:
    switch (funct3) {
    case 0b000:  // SUB
      rv->X[rd] = (int32_t)(rv->X[rs1]) - (int32_t)(rv->X[rs2]);
      break;
    case 0b101:  // SRA
      rv->X[rd] = ((int32_t)rv->X[rs1]) >> (rv->X[rs2] & 0x1f);
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

  // step over instruction
  rv->PC += 4;
}

static void op_lui(struct riscv_t *rv, uint32_t inst) {
  // u-type decode
  const uint32_t rd  = _dec_rd(inst);
  const uint32_t val = _dec_utype_imm(inst);
  rv->X[rd] = val;
  // step over instruction
  rv->PC += 4;
}

static void op_branch(struct riscv_t *rv, uint32_t inst) {
  // b-type decode
  const uint32_t func3 = _dec_funct3(inst);
  const int32_t  imm   = _dec_btype_imm(inst);
  const uint32_t rs1   = _dec_rs1(inst);
  const uint32_t rs2   = _dec_rs2(inst);
  // track if branch is taken or not
  bool taken = false;
  // dispatch by branch type
  switch (func3) {
  case 0: // BEQ
    taken = (rv->X[rs1] == rv->X[rs2]);
    break;
  case 1: // BNE
    taken = (rv->X[rs1] != rv->X[rs2]);
    break;
  case 4: // BLT
    taken = ((int32_t)rv->X[rs1] < (int32_t)rv->X[rs2]);
    break;
  case 5: // BGE
    taken = ((int32_t)rv->X[rs1] >= (int32_t)rv->X[rs2]);
    break;
  case 6: // BLTU
    taken = (rv->X[rs1] < rv->X[rs2]);
    break;
  case 7: // BGEU
    taken = (rv->X[rs1] >= rv->X[rs2]);
    break;
  default:
    assert(!"unreachable");
  }
  // perform branch action
  if (taken) {
    rv->PC += imm;
    if (rv->PC & 0x3) {
      // raise instruction-address-missaligned exception
    }
  }
  else {
    rv->PC += 4;
  }
}

static void op_jalr(struct riscv_t *rv, uint32_t inst) {
  // i-type decode
  const uint32_t rd  = _dec_rd(inst);
  const uint32_t rs1 = _dec_rs1(inst);
  const int32_t  imm = _dec_itype_imm(inst);
  // compute return address
  const uint32_t ra = rv->PC + 4;
  // jump
  rv->PC = (rv->X[rs1] + imm) & ~1u;
  // link
  if (rd) {
    rv->X[rd] = ra;
  }
}

static void op_jal(struct riscv_t *rv, uint32_t inst) {
  // j-type decode
  const uint32_t rd  = _dec_rd(inst);
  const uint32_t rel = _dec_jtype_imm(inst);
  // compute return address
  const uint32_t ra = rv->PC + 4;
  rv->PC += rel;
  // link
  if (rd) {
    rv->X[rd] = ra;
  }
}

static void op_system(struct riscv_t *rv, uint32_t inst) {
  // i-type decode
  const int32_t  imm    = _dec_itype_imm(inst);
  const uint32_t funct3 = _dec_funct3(inst);
  const uint32_t rs1    = _dec_rs1(inst);
  const uint32_t rd     = _dec_rd(inst);
  // dispatch by func3 field
  switch (funct3) {
  case 0:
    // dispatch from imm field
    switch (imm) {
    case 0: // ECALL
      rv->io.on_ecall(rv, rv->PC, inst);
      break;
    case 1: // EBREAK
      rv->io.on_ebreak(rv, rv->PC, inst);
      break;
    default:
      assert(!"unreachable");
    }
    break;
  case 1: // CSRRW    (Atomic Read/Write CSR)
  case 2: // CSRRS    (Atomic Read and Set Bits in CSR)
  case 3: // CSRRC    (Atomic Read and Clear Bits in CSR)
    // TODO
    break;
  case 5: // CSRRWI
  case 6: // CSRRSI
  case 7: // CSRRCI
    // TODO
    break;
  default:
    assert(!"unreachable");
  }
  // step over instruction
  rv->PC += 4;
}

// opcode dispatch table
static const opcode_t opcodes[] = {
  op_load, NULL,  NULL, op_misc_mem, op_op_imm, op_auipc, NULL, NULL, op_store,  NULL,    NULL, NULL,   op_op,     op_lui, NULL, NULL,
  NULL,    NULL,  NULL, NULL,        NULL,      NULL,     NULL, NULL, op_branch, op_jalr, NULL, op_jal, op_system, NULL,   NULL, NULL,
};

struct riscv_t *rv_create(const struct riscv_io_t *io, riscv_user_t userdata) {
  assert(io);
  struct riscv_t *rv = (struct riscv_t *)malloc(sizeof(struct riscv_t));
  // copy over the IO interface
  memcpy(&rv->io, io, sizeof(struct riscv_io_t));
  // copy over the userdata
  rv->userdata = userdata;
  // reset
  rv_reset(rv, 0u);
  return rv;
}

void rv_step(struct riscv_t *rv) {
  assert(rv);
  const uint32_t inst = rv->io.mem_read_w(rv, rv->PC);
  const uint32_t index = (inst & (inst_4_2 | inst_6_5)) >> 2;
  const opcode_t op = opcodes[index];
  if (op) {
    op(rv, inst);
    rv->X[0] = 0;
  }
  else {
    // try to skip over unknown opcode
    rv->PC += 4;
  }
  return;
}

void rv_delete(struct riscv_t *rv) {
  assert(rv);
  free(rv);
  return;
}

void rv_reset(struct riscv_t *rv, riscv_word_t pc) {
  assert(rv);
  memset(rv->X, 0, sizeof(uint32_t) * RV_NUM_REGS);
  // set the reset address
  rv->PC = pc;
  return;
}

riscv_user_t rv_userdata(struct riscv_t *rv) {
  assert(rv);
  return rv->userdata;
}

void rv_set_pc(struct riscv_t *rv, riscv_word_t pc) {
  assert(rv);
  rv->PC = pc;
}

void rv_get_pc(struct riscv_t *rv, riscv_word_t *out) {
  assert(rv);
  assert(out);
  *out = rv->PC;
}

void rv_set_reg(struct riscv_t *rv, uint32_t reg, riscv_word_t in) {
  assert(rv);
  if (reg < RV_NUM_REGS) {
    rv->X[reg] = in;
  }
}

void rv_get_reg(struct riscv_t *rv, uint32_t reg, riscv_word_t *out) {
  assert(rv);
  assert(out);
  if (reg < RV_NUM_REGS) {
    *out = rv->X[reg];
  }
}
