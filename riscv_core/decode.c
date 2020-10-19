#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "riscv.h"
#include "riscv_private.h"
#include "decode.h"


static bool op_load( uint32_t inst, struct rv_inst_t *ir) {

  // itype format
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rd     = dec_rd(inst);

  ir->rd  = rd;
  ir->imm = imm;
  ir->rs1 = rs1;

  switch (funct3) {
  case 0: // LB
    ir->opcode = rv_inst_lb;
    break;
  case 1: // LH
    ir->opcode = rv_inst_lh;
    break;
  case 2: // LW
    ir->opcode = rv_inst_lw;
    break;
  case 4: // LBU
    ir->opcode = rv_inst_lbu;
    break;
  case 5: // LHU
    ir->opcode = rv_inst_lhu;
    break;
  default:
    return false;
  }

  return true;
}

static bool op_op_imm( uint32_t inst, struct rv_inst_t *ir) {

  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->imm = imm;

  // dispatch operation type
  switch (funct3) {
  case 0: // ADDI
    ir->opcode = rv_inst_addi;
    break;
  case 1: // SLLI
    ir->opcode = rv_inst_slli;
    break;
  case 2: // SLTI
    ir->opcode = rv_inst_slti;
    break;
  case 3: // SLTIU
    ir->opcode = rv_inst_sltiu;
    break;
  case 4: // XORI
    ir->opcode = rv_inst_xori;
    break;
  case 5:
    if (imm & ~0x1f) {
      // SRAI
      ir->opcode = rv_inst_srai;
    }
    else {
      // SRLI
      ir->opcode = rv_inst_srli;
    }
    break;
  case 6: // ORI
    ir->opcode = rv_inst_ori;
    break;
  case 7: // ANDI
    ir->opcode = rv_inst_andi;
    break;
  default:
    return false;
  }

  return true;
}

// add upper immediate to pc
static bool op_auipc( uint32_t inst, struct rv_inst_t *ir) {

  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t imm = dec_utype_imm(inst);

  ir->rd  = rd;
  ir->imm = imm;

  ir->opcode = rv_inst_auipc;

  return true;
}

static bool op_store( uint32_t inst, struct rv_inst_t *ir) {

  // s-type format
  const int32_t  imm    = dec_stype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct3 = dec_funct3(inst);

  ir->rd  = rv_reg_zero;
  ir->imm = imm;
  ir->rs1 = rs1;
  ir->rs2 = rs2;

  switch (funct3) {
  case 0: // SB
    ir->opcode = rv_inst_sb;
    break;
  case 1: // SH
    ir->opcode = rv_inst_sh;
    break;
  case 2: // SW
    ir->opcode = rv_inst_sw;
    break;
  default:
    return false;
  }

  return true;
}

static bool op_op( uint32_t inst, struct rv_inst_t *ir) {

  // r-type decode
  const uint32_t rd     = dec_rd(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct7 = dec_funct7(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->rs2 = rs2;

  switch (funct7) {
  case 0b0000000:
    switch (funct3) {
    case 0b000: // ADD
      ir->opcode = rv_inst_add;
      break;
    case 0b001: // SLL
      ir->opcode = rv_inst_sll;
      break;
    case 0b010: // SLT
      ir->opcode = rv_inst_slt;
      break;
    case 0b011: // SLTU
      ir->opcode = rv_inst_sltu;
      break;
    case 0b100: // XOR
      ir->opcode = rv_inst_xor;
      break;
    case 0b101: // SRL
      ir->opcode = rv_inst_srl;
      break;
    case 0b110: // OR
      ir->opcode = rv_inst_or;
      break;
    case 0b111: // AND
      ir->opcode = rv_inst_and;
      break;
    default:
      return false;
    }
    break;
  case 0b0100000:
    switch (funct3) {
    case 0b000: // SUB
      ir->opcode = rv_inst_sub;
      break;
    case 0b101: // SRA
      ir->opcode = rv_inst_sra;
      break;
    default:
      return false;
    }
    break;
#if RISCV_VM_SUPPORT_RV32M
  case 0b0000001:
    // RV32M instructions
    switch (funct3) {
    case 0b000: // MUL
      ir->opcode = rv_inst_mul;
      break;
    case 0b001: // MULH
      ir->opcode = rv_inst_mulh;
      break;
    case 0b011: // MULHU
      ir->opcode = rv_inst_mulhu;
      break;
    case 0b010: // MULHSU
      ir->opcode = rv_inst_mulhsu;
      break;
    case 0b100: // DIV
      ir->opcode = rv_inst_div;
      break;
    case 0b101: // DIVU
      ir->opcode = rv_inst_divu;
      break;
    case 0b110: // REM
      ir->opcode = rv_inst_rem;
      break;
    case 0b111: // REMU
      ir->opcode = rv_inst_remu;
      break;
    default:
      return false;
    }
    break;
#endif  // RISCV_VM_SUPPORT_RV32M
  default:
    return false;
  }

  return true;
}

static bool op_lui(uint32_t inst, struct rv_inst_t *ir) {

  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t val = dec_utype_imm(inst);

  ir->rd  = rd;
  ir->imm = val;

  ir->opcode = rv_inst_lui;

  return true;
}

static bool op_branch(uint32_t inst, struct rv_inst_t *ir) {

  // b-type decode
  const uint32_t func3 = dec_funct3(inst);
  const int32_t  imm   = dec_btype_imm(inst);
  const uint32_t rs1   = dec_rs1(inst);
  const uint32_t rs2   = dec_rs2(inst);

  ir->rd = rv_reg_zero;
  ir->imm = imm;
  ir->rs1 = rs1;
  ir->rs2 = rs2;

  switch (func3) {
  case 0: // BEQ
    ir->opcode = rv_inst_beq;
    break;
  case 1: // BNE
    ir->opcode = rv_inst_bne;
    break;
  case 4: // BLT
    ir->opcode = rv_inst_blt;
    break;
  case 5: // BGE
    ir->opcode = rv_inst_bge;
    break;
  case 6: // BLTU
    ir->opcode = rv_inst_bltu;
    break;
  case 7: // BGEU
    ir->opcode = rv_inst_bgeu;
    break;
  default:
    return false;
  }
  
  return true;
}

static bool op_jalr(uint32_t inst, struct rv_inst_t *ir) {

  // i-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t  imm = dec_itype_imm(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->imm = imm;

  ir->opcode = rv_inst_jalr;

  return true;
}

static bool op_jal(uint32_t inst, struct rv_inst_t *ir) {

  // j-type decode
  const uint32_t rd = dec_rd(inst);
  const int32_t rel = dec_jtype_imm(inst);

  ir->rd  = rd;
  ir->imm = rel;

  ir->opcode = rv_inst_jal;

  return true;
}

static bool op_system(uint32_t inst, struct rv_inst_t *ir) {

  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const int32_t  csr    = dec_csr(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rd     = dec_rd(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;

  // dispatch by func3 field
  switch (funct3) {
  case 0:
    // dispatch from imm field
    switch (imm) {
    case 0: // ECALL
      ir->opcode = rv_inst_ecall;
      break;
    case 1: // EBREAK
      ir->opcode = rv_inst_ebreak;
      break;
    default:
      return false;
    }
    break;
#if RISCV_VM_SUPPORT_Zicsr
  case 1: // CSRRW    (Atomic Read/Write CSR)
    ir->opcode = rv_inst_csrrw;
    break;
  case 2: // CSRRS    (Atomic Read and Set Bits in CSR)
    ir->opcode = rv_inst_csrrs;
    break;
  case 3: // CSRRC    (Atomic Read and Clear Bits in CSR)
    ir->opcode = rv_inst_csrrc;
    break;
  case 5: // CSRRWI
    ir->opcode = rv_inst_csrrwi;
    break;
  case 6: // CSRRSI
    ir->opcode = rv_inst_csrrsi;
    break;
  case 7: // CSRRCI
    ir->opcode = rv_inst_csrrci;
    break;
#endif  // RISCV_VM_SUPPORT_Zicsr
  default:
    return false;
  }

  return true;
}

#if RISCV_VM_SUPPORT_RV32F
static bool op_load_fp(uint32_t inst, struct rv_inst_t *ir) {

  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t  imm = dec_itype_imm(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->imm = imm;

  ir->opcode = rv_inst_flw;

  return true;
}

static bool op_store_fp(uint32_t inst, struct rv_inst_t *ir) {

  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const int32_t  imm = dec_stype_imm(inst);

  ir->rd = rv_reg_zero;
  ir->rs1 = rs1;
  ir->rs2 = rs2;
  ir->imm = imm;

  ir->opcode = rv_inst_fsw;

  return true;
}

static bool op_fp(uint32_t inst, struct rv_inst_t *ir) {

  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t rm     = dec_funct3(inst);
  const uint32_t funct7 = dec_funct7(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->rs2 = rs2;

  // dispatch based on func7 (low 2 bits are width)
  switch (funct7) {
  case 0b0000000:  // FADD
    ir->opcode = rv_inst_fadds;
    break;
  case 0b0000100:  // FSUB
    ir->opcode = rv_inst_fsubs;
    break;
  case 0b0001000:  // FMUL
    ir->opcode = rv_inst_fmuls;
    break;
  case 0b0001100:  // FDIV
    ir->opcode = rv_inst_fdivs;
    break;
  case 0b0101100:  // FSQRT
    ir->opcode = rv_inst_fsqrts;
    break;
  case 0b1100000:
    switch (rs2) {
    case 0b00000:  // FCVT.W.S
      ir->opcode = rv_inst_fcvtws;
      break;
    case 0b00001:  // FCVT.WU.S
      ir->opcode = rv_inst_fcvtwus;
      break;
    default:
      return false;
    }
    break;
  case 0b1110000:
    switch (rm) {
    case 0b000:  // FMV.X.W
      ir->opcode = rv_inst_fmvxw;
      break;
    case 0b001:  // FCLASS.S
      ir->opcode = rv_inst_fclasss;
      break;
    default:
      return false;
    }
    break;
  case 0b1101000:
    switch (rs2) {
    case 0b00000:  // FCVT.S.W
      ir->opcode = rv_inst_fcvtsw;
      break;
    case 0b00001:  // FCVT.S.WU
      ir->opcode = rv_inst_fcvtswu;
      break;
    default:
      return false;
    }
    break;
  case 0b1111000:  // FMV.W.X
    ir->opcode = rv_inst_fmvwx;
    break;
  case 0b0010000: // FSGNJ.S, FSGNJN.S, FSGNJX.S
    switch (rm) {
    case 0b000:  // FSGNJ.S
      ir->opcode = rv_inst_fsgnjs;
      break;
    case 0b001:  // FSGNJN.S
      ir->opcode = rv_inst_fsgnjns;
      break;
    case 0b010:  // FSGNJX.S
      ir->opcode = rv_inst_fsgnjxs;
      break;
    default:
      return false;
    }
    break;
  case 0b0010100: // FMIN, FMAX
    switch (rm) {
    case 0b000:  // FMIN
      ir->opcode = rv_inst_fmins;
      break;
    case 0b001:  // FMAX
      ir->opcode = rv_inst_fmaxs;
      break;
    default:
      return false;
    }
    break;
  case 0b1010000: // FEQ.S, FLT.S, FLE.S
    switch (rm) {
    case 0b010:  // FEQ.S
      ir->opcode = rv_inst_feqs;
      break;
    case 0b001:  // FLT.S
      ir->opcode = rv_inst_flts;
      break;
    case 0b000:  // FLE.S
      ir->opcode = rv_inst_fles;
      break;
    default:
      return false;
    }
    break;
  default:
    return false;
  }

  return true;
}

static bool op_madd(uint32_t inst, struct rv_inst_t *ir) {

  const uint32_t rd  = dec_rd(inst);
  const uint32_t rm  = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->rs2 = rs2;
  ir->rs3 = rs3;

  ir->opcode = rv_inst_fmadds;

  return true;
}

static bool op_msub(uint32_t inst, struct rv_inst_t *ir) {

  const uint32_t rd  = dec_rd(inst);
  const uint32_t rm  = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->rs2 = rs2;
  ir->rs3 = rs3;

  ir->opcode = rv_inst_fmsubs;

  return true;
}

static bool op_nmadd(uint32_t inst, struct rv_inst_t *ir) {

  const uint32_t rd  = dec_rd(inst);
  const uint32_t rm  = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->rs2 = rs2;
  ir->rs3 = rs3;

  ir->opcode = rv_inst_fnmadds;

  return true;
}

static bool op_nmsub(uint32_t inst, struct rv_inst_t *ir) {

  const uint32_t rd  = dec_rd(inst);
  const uint32_t rm  = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);

  ir->rd  = rd;
  ir->rs1 = rs1;
  ir->rs2 = rs2;
  ir->rs3 = rs3;

  ir->opcode = rv_inst_fnmsubs;

  return true;
}
#else
#define op_load_fp  NULL
#define op_store_fp NULL
#define op_fp       NULL
#define op_madd     NULL
#define op_msub     NULL
#define op_nmadd    NULL
#define op_nmsub    NULL
#endif

// opcode handler type
typedef bool(*opcode_t)(uint32_t inst, struct rv_inst_t *ir);

// opcode dispatch table
static const opcode_t opcodes[] = {
  //  000        001          010       011          100        101       110   111
      op_load,   op_load_fp,  NULL,     NULL,        op_op_imm, op_auipc, NULL, NULL, // 00
      op_store,  op_store_fp, NULL,     NULL,        op_op,     op_lui,   NULL, NULL, // 01
      op_madd,   op_msub,     op_nmsub, op_nmadd,    op_fp,     NULL,     NULL, NULL, // 10
      op_branch, op_jalr,     NULL,     op_jal,      op_system, NULL,     NULL, NULL, // 11
};

bool decode(uint32_t inst, struct rv_inst_t *out, uint32_t *pc) {

  // standard uncompressed
  if ((inst & 3) == 3) {
    const uint32_t index = (inst & INST_6_2) >> 2;
    // find translation function
    const opcode_t op = opcodes[index];
    if (!op) {
      return false;
    }
    if (!op(inst, out)) {
      return false;
    }
    *pc += 4;
  }
  else {
    // compressed instruction
    // TODO
    assert(!"Unreachable");
  }

  // success
  return true;
}
