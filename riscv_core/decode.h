#pragma once
#include <stdint.h>
#include <stdbool.h>

#pragma once

enum {
  // RV32I
  rv_inst_lui,
  rv_inst_auipc,
  rv_inst_jal,
  rv_inst_jalr,
  rv_inst_beq,
  rv_inst_bne,
  rv_inst_blt,
  rv_inst_bge,
  rv_inst_bltu,
  rv_inst_bgeu,
  rv_inst_lb,
  rv_inst_lh,
  rv_inst_lw,
  rv_inst_lbu,
  rv_inst_lhu,
  rv_inst_sb,
  rv_inst_sh,
  rv_inst_sw,
  rv_inst_addi,
  rv_inst_slti,
  rv_inst_sltiu,
  rv_inst_xori,
  rv_inst_ori,
  rv_inst_andi,
  rv_inst_slli,
  rv_inst_srli,
  rv_inst_srai,
  rv_inst_add,
  rv_inst_sub,
  rv_inst_sll,
  rv_inst_slt,
  rv_inst_sltu,
  rv_inst_xor,
  rv_inst_srl,
  rv_inst_sra,
  rv_inst_or,
  rv_inst_and,
  rv_inst_fence,
  rv_inst_ecall,
  rv_inst_ebreak,

  // RV32M
  rv_inst_mul,
  rv_inst_mulh,
  rv_inst_mulhsu,
  rv_inst_mulhu,
  rv_inst_div,
  rv_inst_divu,
  rv_inst_rem,
  rv_inst_remu,

  // RV32F
  rv_inst_flw,
  rv_inst_fsw,
  rv_inst_fmadds,
  rv_inst_fmsubs,
  rv_inst_fnmsubs,
  rv_inst_fnmadds,
  rv_inst_fadds,
  rv_inst_fsubs,
  rv_inst_fmuls,
  rv_inst_fdivs,
  rv_inst_fsqrts,
  rv_inst_fsgnjs,
  rv_inst_fsgnjns,
  rv_inst_fsgnjxs,
  rv_inst_fmins,
  rv_inst_fmaxs,
  rv_inst_fcvtws,
  rv_inst_fcvtwus,
  rv_inst_fmvxw,
  rv_inst_feqs,
  rv_inst_flts,
  rv_inst_fles,
  rv_inst_fclasss,
  rv_inst_fcvtsw,
  rv_inst_fcvtswu,
  rv_inst_fmvwx,

  // RV32 Zicsr
  rv_inst_csrrw,
  rv_inst_csrrs,
  rv_inst_csrrc,
  rv_inst_csrrwi,
  rv_inst_csrrsi,
  rv_inst_csrrci,

  // RV32 Zifencei
  rv_inst_fencei,

  // RV32A
  rv_inst_lrw,
  rv_inst_scw,
  rv_inst_amoswapw,
  rv_inst_amoaddw,
  rv_inst_amoxorw,
  rv_inst_amoandw,
  rv_inst_amoorw,
  rv_inst_amominw,
  rv_inst_amomaxw,
  rv_inst_amominuw,
  rv_inst_amomaxuw,
};

struct rv_inst_t {
  uint8_t opcode;
  uint8_t rd, rs1, rs2;
  union {
    int32_t imm;
    uint8_t rs3;
  };
};

static bool inst_is_branch(const struct rv_inst_t *ir) {
  switch (ir->opcode) {
  case rv_inst_jal:
  case rv_inst_jalr:
  case rv_inst_beq:
  case rv_inst_bne:
  case rv_inst_blt:
  case rv_inst_bge:
  case rv_inst_bltu:
  case rv_inst_bgeu:
  case rv_inst_ebreak:
  case rv_inst_ecall:
    return true;
  default:
    return false;
  }
}

static bool inst_is_rv32f(const struct rv_inst_t *ir) {
  switch (ir->opcode) {
  case rv_inst_flw:
  case rv_inst_fsw:
  case rv_inst_fmadds:
  case rv_inst_fmsubs:
  case rv_inst_fnmsubs:
  case rv_inst_fnmadds:
  case rv_inst_fadds:
  case rv_inst_fsubs:
  case rv_inst_fmuls:
  case rv_inst_fdivs:
  case rv_inst_fsqrts:
  case rv_inst_fsgnjs:
  case rv_inst_fsgnjns:
  case rv_inst_fsgnjxs:
  case rv_inst_fmins:
  case rv_inst_fmaxs:
  case rv_inst_fcvtws:
  case rv_inst_fcvtwus:
  case rv_inst_fmvxw:
  case rv_inst_feqs:
  case rv_inst_flts:
  case rv_inst_fles:
  case rv_inst_fclasss:
  case rv_inst_fcvtsw:
  case rv_inst_fcvtswu:
  case rv_inst_fmvwx:
    return true;
  }
  return false;
}

static bool inst_bypass_zero_store(const struct rv_inst_t *inst) {
  switch (inst->opcode) {
  case rv_inst_jal:
  case rv_inst_jalr:
  case rv_inst_beq:
  case rv_inst_bne:
  case rv_inst_blt:
  case rv_inst_bge:
  case rv_inst_bltu:
  case rv_inst_bgeu:
  case rv_inst_sb:
  case rv_inst_sh:
  case rv_inst_sw:
  case rv_inst_ecall:
  case rv_inst_ebreak:
  case rv_inst_flw:
  case rv_inst_fsw:
  case rv_inst_fmadds:
  case rv_inst_fmsubs:
  case rv_inst_fnmsubs:
  case rv_inst_fnmadds:
  case rv_inst_fadds:
  case rv_inst_fsubs:
  case rv_inst_fmuls:
  case rv_inst_fdivs:
  case rv_inst_fsqrts:
  case rv_inst_fsgnjs:
  case rv_inst_fsgnjns:
  case rv_inst_fsgnjxs:
  case rv_inst_fmins:
  case rv_inst_fmaxs:
  case rv_inst_fcvtws:
  case rv_inst_fcvtwus:
  case rv_inst_fmvxw:
  case rv_inst_feqs:
  case rv_inst_flts:
  case rv_inst_fles:
  case rv_inst_fclasss:
  case rv_inst_fcvtsw:
  case rv_inst_fcvtswu:
  case rv_inst_fmvwx:
    return true;
  }
  return false;
}

bool decode(uint32_t inst, struct rv_inst_t *out, uint32_t *pc);

bool codegen(const struct rv_inst_t *ir, struct cg_state_t *cg, uint32_t pc, uint32_t inst);
void codegen_prologue(struct cg_state_t *cg);
void codegen_epilogue(struct cg_state_t *cg);
