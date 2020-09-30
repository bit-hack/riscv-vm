#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "riscv.h"

#if SUPPORT_RV32F
#include <math.h>
#endif


#define RV_NUM_REGS 32


// csrs
enum {
  // floating point
  CSR_FFLAGS    = 0x001,
  CSR_FRM       = 0x002,
  CSR_FCSR      = 0x003,
  // maching status
  CSR_MSTATUS   = 0x300,
  // low words
  CSR_CYCLE     = 0xb00, // 0xC00,
  CSR_TIME      = 0xC01,
  CSR_INSTRET   = 0xC02,
  // high words
  CSR_CYCLEH    = 0xC80,
  CSR_TIMEH     = 0xC81,
  CSR_INSTRETH  = 0xC82
};

// instruction decode masks
enum {
  //               ....xxxx....xxxx....xxxx....xxxx
  INST_6_2     = 0b00000000000000000000000001111100,
  //               ....xxxx....xxxx....xxxx....xxxx
  FR_OPCODE    = 0b00000000000000000000000001111111, // r-type
  FR_RD        = 0b00000000000000000000111110000000,
  FR_FUNCT3    = 0b00000000000000000111000000000000,
  FR_RS1       = 0b00000000000011111000000000000000,
  FR_RS2       = 0b00000001111100000000000000000000,
  FR_FUNCT7    = 0b11111110000000000000000000000000,
  //               ....xxxx....xxxx....xxxx....xxxx
  FI_IMM_11_0  = 0b11111111111100000000000000000000, // i-type
  //               ....xxxx....xxxx....xxxx....xxxx
  FS_IMM_4_0   = 0b00000000000000000000111110000000, // s-type
  FS_IMM_11_5  = 0b11111110000000000000000000000000,
  //               ....xxxx....xxxx....xxxx....xxxx
  FB_IMM_11    = 0b00000000000000000000000010000000, // b-type
  FB_IMM_4_1   = 0b00000000000000000000111100000000,
  FB_IMM_10_5  = 0b01111110000000000000000000000000,
  FB_IMM_12    = 0b10000000000000000000000000000000,
  //               ....xxxx....xxxx....xxxx....xxxx
  FU_IMM_31_12 = 0b11111111111111111111000000000000, // u-type
  //               ....xxxx....xxxx....xxxx....xxxx
  FJ_IMM_19_12 = 0b00000000000011111111000000000000, // j-type
  FJ_IMM_11    = 0b00000000000100000000000000000000,
  FJ_IMM_10_1  = 0b01111111111000000000000000000000,
  FJ_IMM_20    = 0b10000000000000000000000000000000,
  //               ....xxxx....xxxx....xxxx....xxxx
  FR4_FMT      = 0b00000110000000000000000000000000, // r4-type
  FR4_RS3      = 0b11111000000000000000000000000000,
  //               ....xxxx....xxxx....xxxx....xxxx
};

struct riscv_t {
  // io interface
  struct riscv_io_t io;
  // integer registers
  riscv_word_t X[RV_NUM_REGS];
  riscv_word_t PC;
  // user provided data
  riscv_user_t userdata;
  // exception status
  riscv_exception_t exception;
  // CSRs
  uint64_t csr_cycle;
  uint32_t csr_mstatus;
#if SUPPORT_RV32F
  // float registers
  riscv_float_t F[RV_NUM_REGS];
  uint32_t csr_fcsr;
#endif
};

// decode rd field
static inline uint32_t dec_rd(uint32_t inst) {
  return (inst & FR_RD) >> 7;
}

// decode rs1 field
static inline uint32_t dec_rs1(uint32_t inst) {
  return (inst & FR_RS1) >> 15;
}

// decode rs2 field
static inline uint32_t dec_rs2(uint32_t inst) {
  return (inst & FR_RS2) >> 20;
}

// decoded funct3 field
static inline uint32_t dec_funct3(uint32_t inst) {
  return (inst & FR_FUNCT3) >> 12;
}

// decode funct7 field
static inline uint32_t dec_funct7(uint32_t inst) {
  return (inst & FR_FUNCT7) >> 25;
}

// decode utype instruction immediate
static inline uint32_t dec_utype_imm(uint32_t inst) {
  return inst & FU_IMM_31_12;
}

// decode jtype instruction immediate
static inline int32_t dec_jtype_imm(uint32_t inst) {
  uint32_t dst = 0;
  dst |= (inst & FJ_IMM_20);
  dst |= (inst & FJ_IMM_19_12) << 11;
  dst |= (inst & FJ_IMM_11)    << 2;
  dst |= (inst & FJ_IMM_10_1)  >> 9;
  // note: shifted to 2nd least significant bit
  return ((int32_t)dst) >> 11;
}

// decode itype instruction immediate
static inline int32_t dec_itype_imm(uint32_t inst) {
  return ((int32_t)(inst & FI_IMM_11_0)) >> 20;
}

// decode r4type format field
static inline uint32_t dec_r4type_fmt(uint32_t inst) {
  return (inst & FR4_FMT) >> 25;
}

// decode r4type rs3 field
static inline uint32_t dec_r4type_rs3(uint32_t inst) {
  return (inst & FR4_RS3) >> 27;
}

// decode csr instruction immediate (same as itype, zero extend)
static inline uint32_t dec_csr(uint32_t inst) {
  return ((uint32_t)(inst & FI_IMM_11_0)) >> 20;
}

// decode btype instruction immediate
static inline int32_t dec_btype_imm(uint32_t inst) {
  uint32_t dst = 0;
  dst |= (inst & FB_IMM_12);
  dst |= (inst & FB_IMM_11) << 23;
  dst |= (inst & FB_IMM_10_5) >> 1;
  dst |= (inst & FB_IMM_4_1) << 12;
  // note: shifted to 2nd least significant bit
  return ((int32_t)dst) >> 19;
}

// decode stype instruction immediate
static inline int32_t dec_stype_imm(uint32_t inst) {
  uint32_t dst = 0;
  dst |= (inst & FS_IMM_11_5);
  dst |= (inst & FS_IMM_4_0) << 13;
  return ((int32_t)dst) >> 20;
}

// sign extend a 16 bit value
static inline uint32_t sign_extend_h(uint32_t x) {
  return (int32_t)((int16_t)x);
}

// sign extend an 8 bit value
static inline uint32_t sign_extend_b(uint32_t x) {
  return (int32_t)((int8_t)x);
}

// get a pointer to a CSR
static uint32_t *csr_get_ptr(struct riscv_t *rv, uint32_t csr) {
  switch (csr) {
  case CSR_CYCLE:
    return (uint32_t*)(&rv->csr_cycle) + 0;
  case CSR_CYCLEH:
    return (uint32_t*)(&rv->csr_cycle) + 1;
  case CSR_MSTATUS:
    return (uint32_t*)(&rv->csr_mstatus);
#if SUPPORT_RV32F
  case CSR_FCSR:
    return (uint32_t*)(&rv->csr_fcsr);
#endif
  default:
    return NULL;
  }
}

static bool csr_is_writable(uint32_t csr) {
  switch (csr) {
  case CSR_MSTATUS:
    return true;
  case CSR_CYCLE:
  case CSR_CYCLEH:
  case CSR_FCSR:
  default:
    return false;
  }
}

// perform csrrw
static uint32_t csr_csrrw(struct riscv_t *rv, uint32_t csr, uint32_t val) {
  uint32_t *c = csr_get_ptr(rv, csr);
  if (!c) {
    return 0;
  }
  const uint32_t out = *c;
  if (csr_is_writable(csr)) {
    *c = val;
  }
  return out;
}

// perform csrrs (atomic read and set)
static uint32_t csr_csrrs(struct riscv_t *rv, uint32_t csr, uint32_t val) {
  uint32_t *c = csr_get_ptr(rv, csr);
  if (!c) {
    return 0;
  }
  const uint32_t out = *c;
  if (csr_is_writable(csr)) {
    *c |= val;
  }
  return out;
}

// perform csrrc (atomic read and clear)
static uint32_t csr_csrrc(struct riscv_t *rv, uint32_t csr, uint32_t val) {
  uint32_t *c = csr_get_ptr(rv, csr);
  if (!c) {
    return 0;
  }
  const uint32_t out = *c;
  if (csr_is_writable(csr)) {
    *c &= ~val;
  }
  return out;
}

// raise an exception in the processor
static inline void raise_exception(struct riscv_t *rv, uint32_t type) {
  rv->exception = type;
}

// opcode handler type
typedef void(*opcode_t)(struct riscv_t *rv, uint32_t inst);

static void op_load(struct riscv_t *rv, uint32_t inst) {
  // itype format
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rd     = dec_rd(inst);
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
  // enforce zero register
  if (rd == rv_reg_zero) {
    rv->X[rv_reg_zero] = 0;
  }
}

#if SUPPORT_Zifencei
static void op_misc_mem(struct riscv_t *rv, uint32_t inst) {
  // TODO
  rv->PC += 4;
}
#else
#define op_misc_mem NULL
#endif  // SUPPORT_Zifencei

static void op_op_imm(struct riscv_t *rv, uint32_t inst) {
  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);
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
  // enforce zero register
  if (rd == rv_reg_zero) {
    rv->X[rv_reg_zero] = 0;
  }
}

// add upper immediate to pc
static void op_auipc(struct riscv_t *rv, uint32_t inst) {
  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t val = dec_utype_imm(inst) + rv->PC;
  rv->X[rd] = val;
  // step over instruction
  rv->PC += 4;
  // enforce zero register
  if (rd == rv_reg_zero) {
    rv->X[rv_reg_zero] = 0;
  }
}

static void op_store(struct riscv_t *rv, uint32_t inst) {
  // s-type format
  const int32_t  imm    = dec_stype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct3 = dec_funct3(inst);
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
  const uint32_t rd     = dec_rd(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct7 = dec_funct7(inst);

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
  // enforce zero register
  if (rd == rv_reg_zero) {
    rv->X[rv_reg_zero] = 0;
  }
}

static void op_lui(struct riscv_t *rv, uint32_t inst) {
  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t val = dec_utype_imm(inst);
  rv->X[rd] = val;
  // step over instruction
  rv->PC += 4;
  // enforce zero register
  if (rd == rv_reg_zero) {
    rv->X[rv_reg_zero] = 0;
  }
}

static void op_branch(struct riscv_t *rv, uint32_t inst) {
  // b-type decode
  const uint32_t func3 = dec_funct3(inst);
  const int32_t  imm   = dec_btype_imm(inst);
  const uint32_t rs1   = dec_rs1(inst);
  const uint32_t rs2   = dec_rs2(inst);
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
      raise_exception(rv, rv_except_inst_misaligned);
    }
  }
  else {
    // step over instruction
    rv->PC += 4;
  }
}

static void op_jalr(struct riscv_t *rv, uint32_t inst) {
  // i-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t  imm = dec_itype_imm(inst);
  // compute return address
  const uint32_t ra = rv->PC + 4;
  // jump
  rv->PC = (rv->X[rs1] + imm) & ~1u;
  // link
  if (rd != rv_reg_zero) {
    rv->X[rd] = ra;
  }
  // check for exception
  if (rv->PC & 0x3) {
    raise_exception(rv, rv_except_inst_misaligned);
  }
}

static void op_jal(struct riscv_t *rv, uint32_t inst) {
  // j-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rel = dec_jtype_imm(inst);
  // compute return address
  const uint32_t ra = rv->PC + 4;
  rv->PC += rel;
  // link
  if (rd != rv_reg_zero) {
    rv->X[rd] = ra;
  }
  // check alignment of PC
  if (rv->PC & 0x3) {
    raise_exception(rv, rv_except_inst_misaligned);
  }
}

static void op_system(struct riscv_t *rv, uint32_t inst) {
  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const int32_t  csr    = dec_csr(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rd     = dec_rd(inst);
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
#if SUPPORT_Zicsr
  case 1: // CSRRW    (Atomic Read/Write CSR)
    rv->X[rd] = csr_csrrw(rv, csr, rs1);
    break;
  case 2: // CSRRS    (Atomic Read and Set Bits in CSR)
    rv->X[rd] = csr_csrrs(rv, csr, rs1);
    break;
  case 3: // CSRRC    (Atomic Read and Clear Bits in CSR)
    rv->X[rd] = csr_csrrc(rv, csr, rs1);
    break;
  case 5: // CSRRWI
  case 6: // CSRRSI
  case 7: // CSRRCI
    // TODO
    break;
#endif  // SUPPORT_Zicsr
  default:
    assert(!"unreachable");
  }
  // step over instruction
  rv->PC += 4;
  // enforce zero register
  if (rd == rv_reg_zero) {
    rv->X[rv_reg_zero] = 0;
  }
}

#if SUPPORT_RV32A
static void op_amo(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t f7     = dec_funct7(inst);
  const uint32_t rl     = (f7 >> 0) & 1;
  const uint32_t aq     = (f7 >> 1) & 1;
  const uint32_t funct5 = (f7 >> 2) & 0x1f;

  switch (funct5) {
  case 0b00010:  // LR.W
    rv->X[rd] = rv->io.mem_read_w(rv, rv->X[rs1]);
    // skip registration of the 'reservation set'
    // TODO: implement me
    break;
  case 0b00011:  // SC.W
    // we assume the 'reservation set' is valid
    // TODO: implement me
    rv->io.mem_write_w(rv, rv->X[rs1], rv->X[rs2]);
    rv->X[rd] = 0;
    break;
  case 0b00001:  // AMOSWAP.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      rv->io.mem_write_s(rv, rs1, rv->X[rs2]);
      break;
    }
  case 0b00000:  // AMOADD.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      const int32_t res = (int32_t)rv->X[rd] + (int32_t)rv->X[rs2];
      rv->io.mem_write_s(rv, rs1, res);
      break;
    }
  case 0b00100:  // AMOXOR.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      const int32_t res = rv->X[rd] ^ rv->X[rs2];
      rv->io.mem_write_s(rv, rs1, res);
      break;
    }
  case 0b01100:  // AMOAND.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      const int32_t res = rv->X[rd] & rv->X[rs2];
      rv->io.mem_write_s(rv, rs1, res);
      break;
    }
  case 0b01000:  // AMOOR.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      const int32_t res = rv->X[rd] | rv->X[rs2];
      rv->io.mem_write_s(rv, rs1, res);
      break;
    }
  case 0b10000:  // AMOMIN.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      const int32_t a = rv->X[rd];
      const int32_t b = rv->X[rs2];
      const int32_t res = a < b ? a : b;
      rv->io.mem_write_s(rv, rs1, res);
      break;
    }
  case 0b10100:  // AMOMAX.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      const int32_t a = rv->X[rd];
      const int32_t b = rv->X[rs2];
      const int32_t res = a > b ? a : b;
      rv->io.mem_write_s(rv, rs1, res);
      break;
    }
  case 0b11000:  // AMOMINU.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      const uint32_t a = rv->X[rd];
      const uint32_t b = rv->X[rs2];
      const uint32_t res = a < b ? a : b;
      rv->io.mem_write_s(rv, rs1, res);
      break;
    }
  case 0b11100:  // AMOMAXU.W
    {
      rv->X[rd] = rv->io.mem_read_w(rv, rs1);
      const uint32_t a = rv->X[rd];
      const uint32_t b = rv->X[rs2];
      const uint32_t res = a > b ? a : b;
      rv->io.mem_write_s(rv, rs1, res);
      break;
    }
  default:
    assert(!"unreachable");
  }
  // step over instruction
  rv->PC += 4;
  // enforce zero register
  if (rd == rv_reg_zero) {
    rv->X[rv_reg_zero] = 0;
  }
}
#else
#define op_amo NULL
#endif // SUPPORT_RV32A

#if SUPPORT_RV32F
enum {
  //             ....xxxx....xxxx....xxxx....xxxx
  FMASK_SIGN = 0b10000000000000000000000000000000,
  FMASK_EXPN = 0b01111111100000000000000000000000,
  FMASK_FRAC = 0b00000000011111111111111111111111,
  //             ........xxxxxxxx........xxxxxxxx
};

static uint32_t calc_fclass(uint32_t f) {
  const uint32_t sign = f & FMASK_SIGN;
  const uint32_t expn = f & FMASK_EXPN;
  const uint32_t frac = f & FMASK_FRAC;

  // note: this could be turned into a binary decision tree for speed

  uint32_t out = 0;
  // 0x001    rs1 is -INF
  out |= (f == 0xff800000)                               ? 0x001 : 0;
  // 0x002    rs1 is negative normal
  out |= (expn && expn < 0x78000000 && sign)             ? 0x002 : 0;
  // 0x004    rs1 is negative subnormal
  out |= (!expn && frac && sign)                         ? 0x004 : 0;
  // 0x008    rs1 is -0
  out |= (f == 0x80000000)                               ? 0x008 : 0;
  // 0x010    rs1 is +0
  out |= (f == 0x00000000)                               ? 0x010 : 0;
  // 0x020    rs1 is positive subnormal
  out |= (!expn && frac && !sign)                        ? 0x020 : 0;
  // 0x040    rs1 is positive normal
  out |= (expn && expn < 0x78000000 && !sign)            ? 0x040 : 0;
  // 0x080    rs1 is +INF
  out |= (f == 0x7f800000)                               ? 0x080 : 0;
  // 0x100    rs1 is a signaling NaN
  out |= (expn == FMASK_EXPN && (frac <= 0x7ff) && frac) ? 0x100 : 0;
  // 0x200    rs1 is a quiet NaN
  out |= (expn == FMASK_EXPN && (frac >= 0x800))         ? 0x200 : 0;

  return out;
}

void op_load_fp(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t imm = dec_itype_imm(inst);
  // calculate load address
  const uint32_t addr = rv->X[rs1] + imm;
  // copy into the float register
  const uint32_t data = rv->io.mem_read_w(rv, addr);
  memcpy(rv->F + rd, &data, 4);
  // step over instruction
  rv->PC += 4;
}

void op_store_fp(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const int32_t imm = dec_stype_imm(inst);
  // calculate store address
  const uint32_t addr = rv->X[rs1] + imm;
  // copy from float registers
  uint32_t data;
  memcpy(&data, (const void*)(rv->F + rs2), 4);
  rv->io.mem_write_w(rv, addr, data);
  // step over instruction
  rv->PC += 4;
}

void op_fp(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t rm     = dec_funct3(inst); // TODO: rounding!
  const uint32_t funct7 = dec_funct7(inst);
  // dispatch based on func7 (low 2 bits are width)
  switch (funct7) {
  case 0b0000000:  // FADD
    rv->F[rd] = rv->F[rs1] + rv->F[rs2];
    break;
  case 0b0000100:  // FSUB
    rv->F[rd] = rv->F[rs1] - rv->F[rs2];
    break;
  case 0b0001000:  // FMUL
    rv->F[rd] = rv->F[rs1] * rv->F[rs2];
    break;
  case 0b0001100:  // FDIV
    rv->F[rd] = rv->F[rs1] / rv->F[rs2];
    break;
  case 0b0101100:  // FSQRT
    rv->F[rd] = sqrtf(rv->F[rs1]);
    break;
  case 0b0010000:
  {
    uint32_t f1, f2, res;
    memcpy(&f1, rv->F + rs1, 4);
    memcpy(&f2, rv->F + rs2, 4);
    switch (rm) {
    case 0b000:  // FSGNJ.S
      res = (f1 & ~FMASK_SIGN) | (f2 & FMASK_SIGN);
      break;
    case 0b001:  // FSGNJN.S
      res = (f1 & ~FMASK_SIGN) | (~f2 & FMASK_SIGN);
      break;
    case 0b010:  // FSGNJX.S
      res = f1 ^ (f2 & FMASK_SIGN);
      break;
    default:
      assert(!"unreachable");
    }
    memcpy(rv->F + rd, &res, 4);
    break;
  }
  case 0b0010100:
    switch (rm) {
    case 0b000:  // FMIN
      rv->F[rd] = fminf(rv->F[rs1], rv->F[rs2]);
      break;
    case 0b001:  // FMAX
      rv->F[rd] = fmaxf(rv->F[rs1], rv->F[rs2]);
      break;
    default:
      assert(!"unreachable");
    }
    break;
  case 0b1100000:
    switch (rs2) {
    case 0b00000:  // FCVT.W.S
      rv->X[rd] = (int32_t)rv->F[rs1];
      break;
    case 0b00001:  // FCVT.WU.S
      rv->X[rd] = (uint32_t)rv->F[rs1];
      break;
    default:
      assert(!"unreachable");
    }
    break;
  case 0b1110000:
    switch (rm) {
    case 0b000:  // FMV.X.W
      // bit exact copy between register files
      memcpy(rv->X + rd, rv->F + rs1, 4);
      break;
    case 0b001:  // FCLASS.S
      {
        uint32_t bits;
        memcpy(&bits, rv->F + rs1, 4);
        rv->X[rd] = calc_fclass(bits);
        break;
      }
    default:
      assert(!"unreachable");
    }
    break;
  case 0b1010000:
    switch (rm) {
    case 0b010:  // FEQ.S
      rv->X[rd] = (rv->F[rs1] == rv->F[rs2]) ? 1 : 0;
      break;
    case 0b001:  // FLT.S
      rv->X[rd] = (rv->F[rs1] < rv->F[rs2]) ? 1 : 0;
      break;
    case 0b000:  // FLE.S
      rv->X[rd] = (rv->F[rs1] <= rv->F[rs2]) ? 1 : 0;
      break;
    default:
      assert(!"unreachable");
    }
    break;
  case 0b1101000:
    switch (rs2) {
    case 0b00000:  // FCVT.S.W
      rv->F[rd] = (float)(int32_t)rv->X[rs1];
      break;
    case 0b00001:  // FCVT.S.WU
      rv->F[rd] = (float)(uint32_t)rv->X[rs1];
      break;
    default:
      assert(!"unreachable");
    }
    break;
  case 0b1111000:  // FMV.W.X
    // bit exact copy between register files
    memcpy(rv->F + rd, rv->X + rs1, 4);
    break;
  default:
    assert(!"unreachable");
  }
  // step over instruction
  rv->PC += 4;
}

void op_madd(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rm  = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);
  // compute
  rv->F[rd] = rv->F[rs1] * rv->F[rs2] + rv->F[rs3];
  // step over instruction
  rv->PC += 4;
}

void op_msub(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rm  = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);
  // compute
  rv->F[rd] = rv->F[rs1] * rv->F[rs2] - rv->F[rs3];
  // step over instruction
  rv->PC += 4;
}

void op_nmsub(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rm  = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);
  // compute
  rv->F[rd] = -(rv->F[rs1] * rv->F[rs2]) + rv->F[rs3];
  // step over instruction
  rv->PC += 4;
}

void op_nmadd(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rm  = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);
  // compute
  rv->F[rd] = -(rv->F[rs1] * rv->F[rs2]) - rv->F[rs3];
  // step over instruction
  rv->PC += 4;
}
#else
#define op_load_fp  NULL
#define op_store_fp NULL
#define op_fp       NULL
#define op_madd     NULL
#define op_msub     NULL
#define op_nmsub    NULL
#define op_nmadd    NULL
#endif  // SUPPORT_RV32F

// opcode dispatch table
static const opcode_t opcodes[] = {
//  000        001          010       011          100        101       110   111
    op_load,   op_load_fp,  NULL,     op_misc_mem, op_op_imm, op_auipc, NULL, NULL, // 00
    op_store,  op_store_fp, NULL,     op_amo,      op_op,     op_lui,   NULL, NULL, // 01
    op_madd,   op_msub,     op_nmsub, op_nmadd,    op_fp,     NULL,     NULL, NULL, // 10
    op_branch, op_jalr,     NULL,     op_jal,      op_system, NULL,     NULL, NULL, // 11
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

void rv_step(struct riscv_t *rv, uint32_t cycles) {
  assert(rv);
  while (cycles-- && !rv->exception) {
    // fetch the next instruction
    const uint32_t inst = rv->io.mem_ifetch(rv, rv->PC);
    const uint32_t index = (inst & INST_6_2) >> 2;
    // dispatch this opcode
    const opcode_t op = opcodes[index];
    assert(op);
    op(rv, inst);
    // increment the cycles csr
    rv->csr_cycle++;
  }
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
  // set the default stack pointer
  rv->X[rv_reg_sp] = DEFAULT_STACK_ADDR;
  // reset exception state
  rv->exception = rv_except_none;
  // reset the csrs
  rv->csr_cycle = 0;
  rv->csr_mstatus = 0;
  // reset float registers
#if SUPPORT_RV32F
  memset(rv->F, 0, sizeof(float) * RV_NUM_REGS);
  rv->csr_fcsr = 0;
#endif
}

riscv_user_t rv_userdata(struct riscv_t *rv) {
  assert(rv);
  return rv->userdata;
}

bool rv_set_pc(struct riscv_t *rv, riscv_word_t pc) {
  assert(rv);
  if (pc & 3) {
    return false;
  }
  rv->PC = pc;
  return true;
}

riscv_word_t rv_get_pc(struct riscv_t *rv) {
  assert(rv);
  return rv->PC;
}

void rv_set_reg(struct riscv_t *rv, uint32_t reg, riscv_word_t in) {
  assert(rv);
  if (reg < RV_NUM_REGS && reg != rv_reg_zero) {
    rv->X[reg] = in;
  }
}

riscv_word_t rv_get_reg(struct riscv_t *rv, uint32_t reg) {
  assert(rv);
  if (reg < RV_NUM_REGS) {
    return rv->X[reg];
  }
  return ~0u;
}

riscv_exception_t rv_get_exception(struct riscv_t *rv) {
  return rv->exception;
}
