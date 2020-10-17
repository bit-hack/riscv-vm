#include <assert.h>
#include <stddef.h>

#include "riscv_private.h"
#include "decode.h"

#include "../tinycg/tinycg.h"


// byte offset from rv structure address to member address
#define rv_offset(RV, MEMBER) ((int32_t)(((uintptr_t)&(RV.MEMBER)) - (uintptr_t)&RV))



bool codegen(const struct rv_inst_t *i, struct cg_state_t *cg, uint32_t pc, uint32_t inst) {

  // this struct is only used for offsets
  static struct riscv_t rv;

  if (i->rd == rv_reg_zero) {
    switch (i->opcode) {
    case rv_inst_jal:
    case rv_inst_jalr:
      break;
    default:
      return true;
    }
  }

  switch (i->opcode) {
  // RV32I
  case rv_inst_lui:
    cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm);
    break;
  case rv_inst_auipc:
    cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), pc + i->imm);
    break;
  case rv_inst_jal:
    cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, PC), pc + i->imm);
    if (i->rd != rv_reg_zero) {
      cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), pc + 4);
    }
    break;
  case rv_inst_jalr:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_add_r32_i32(cg, cg_eax, i->imm);
    cg_and_r32_i32(cg, cg_eax, 0xfffffffe);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, PC), cg_eax);
    if (i->rd != rv_reg_zero) {
      cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), pc + 4);
    }
    break;
  case rv_inst_beq:
  case rv_inst_bne:
  case rv_inst_blt:
  case rv_inst_bge:
  case rv_inst_bltu:
  case rv_inst_bgeu:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, X[i->rs1]));
    cg_cmp_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, X[i->rs2]));
    cg_mov_r32_i32(cg, cg_eax, pc + 4);
    cg_mov_r32_i32(cg, cg_edx, pc + i->imm);
    switch (i->opcode) {
    case rv_inst_beq:
      cg_cmov_r32_r32(cg, cg_cc_eq, cg_eax, cg_edx);
      break;
    case rv_inst_bne:
      cg_cmov_r32_r32(cg, cg_cc_ne, cg_eax, cg_edx);
      break;
    case rv_inst_blt:
      cg_cmov_r32_r32(cg, cg_cc_lt, cg_eax, cg_edx);
      break;
    case rv_inst_bge:
      cg_cmov_r32_r32(cg, cg_cc_ge, cg_eax, cg_edx);
      break;
    case rv_inst_bltu:
      cg_cmov_r32_r32(cg, cg_cc_c, cg_eax, cg_edx);
      break;
    case rv_inst_bgeu:
      cg_cmov_r32_r32(cg, cg_cc_ae, cg_eax, cg_edx);
      break;
    }
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, PC), cg_eax);
    break;
  case rv_inst_lb:
  case rv_inst_lh:
  case rv_inst_lw:
  case rv_inst_lbu:
  case rv_inst_lhu:
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);                                 // rv
    cg_mov_r32_r64disp(cg, cg_edx, cg_rsi, rv_offset(rv, X[i->rs1]));
    cg_add_r32_i32(cg, cg_edx, i->imm);                                 // addr
    switch (i->opcode) {
    case rv_inst_lb:
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_read_b));
      cg_movsx_r32_r8(cg, cg_eax, cg_al);
      break;
    case rv_inst_lh:
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_read_s));
      cg_movsx_r32_r16(cg, cg_eax, cg_ax);
      break;
    case rv_inst_lw:
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_read_w));
      break;
    case rv_inst_lbu:
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_read_b));
      break;
    case rv_inst_lhu:
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_read_s));
      break;
    }
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_sb:
  case rv_inst_sh:
  case rv_inst_sw:
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);                                 // rv
    cg_mov_r32_r64disp(cg, cg_edx, cg_rsi, i->rs1);
    cg_add_r32_i32(cg, cg_edx, i->imm);                                 // addr
    cg_movsx_r64_r64disp(cg, cg_r8, cg_rsi, rv_offset(rv, X[i->rs2]));  // value
    switch (i->opcode) {
    case rv_inst_sb:
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_write_b));       // store
      break;
    case rv_inst_sh:
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_write_s));       // store
      break;
    case rv_inst_sw:
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_write_w));       // store
      break;
    }
    break;

  case rv_inst_addi:
    if (i->rd == i->rs1) {
      cg_add_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm);
    }
    else {
      cg_add_r32_i32(cg, cg_eax, i->imm);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    }
    break;
  case rv_inst_slti:
    cg_cmp_r32_i32(cg, cg_eax, i->imm);
    cg_setcc_r8(cg, cg_cc_lt, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_sltiu:
    cg_cmp_r32_i32(cg, cg_eax, i->imm);
    cg_setcc_r8(cg, cg_cc_c, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_xori:
    if (i->rd == i->rs1) {
      cg_xor_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm);
    }
    else {
      cg_xor_r32_i32(cg, cg_eax, i->imm);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    }
    break;
  case rv_inst_ori:
    if (i->rd == i->rs1) {
      cg_or_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm);
    }
    else {
      cg_or_r32_i32(cg, cg_eax, i->imm);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    }
    break;
  case rv_inst_andi:
    if (i->rd == i->rs1) {
      cg_and_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm);
    }
    else {
      cg_and_r32_i32(cg, cg_eax, i->imm);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    }
    break;
  case rv_inst_slli:
    if (i->rd == i->rs1) {
      cg_shl_r64disp_i8(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm & 0x1f);
    }
    else {
      cg_shl_r32_i8(cg, cg_eax, i->imm & 0x1f);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    }
    break;
  case rv_inst_srli:
    if (i->rd == i->rs1) {
      cg_shr_r64disp_i8(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm & 0x1f);
    }
    else {
      cg_shr_r32_i8(cg, cg_eax, i->imm & 0x1f);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    }
    break;
  case rv_inst_srai:
    if (i->rd == i->rs1) {
      cg_sar_r64disp_i8(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm & 0x1f);
    }
    else {
      cg_sar_r32_i8(cg, cg_eax, i->imm & 0x1f);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    }
    break;

  case rv_inst_add:
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    if (i->rs1 == i->rd) {
      cg_add_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
    }
    else {
      cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
      cg_add_r32_r32(cg, cg_eax, cg_ecx);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    }
    break;
  case rv_inst_sub:
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
//    if (i->rs1 == i->rd) {
//      cg_sub_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
//    }
//    else {
      cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
      cg_sub_r32_r32(cg, cg_eax, cg_ecx);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
//    }
    break;
  case rv_inst_sll:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    cg_and_r8_i8(cg, cg_cl, 0x1f);
    cg_shl_r32_cl(cg, cg_eax);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_slt:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    cg_cmp_r32_r32(cg, cg_eax, cg_ecx);
    cg_setcc_r8(cg, cg_cc_lt, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_sltu:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    cg_cmp_r32_r32(cg, cg_eax, cg_ecx);
    cg_setcc_r8(cg, cg_cc_c, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_xor:
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
//    if (i->rs1 == i->rd) {
//      cg_xor_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
//    }
//    else {
      cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
      cg_xor_r32_r32(cg, cg_eax, cg_ecx);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
//    }
    break;
  case rv_inst_srl:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    cg_and_r8_i8(cg, cg_cl, 0x1f);
    cg_shr_r32_cl(cg, cg_eax);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_sra:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    cg_and_r8_i8(cg, cg_cl, 0x1f);
    cg_sar_r32_cl(cg, cg_eax);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_or:
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
//    if (i->rs1 == i->rd) {
//      cg_or_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
//    }
//    else {
      cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
      cg_or_r32_r32(cg, cg_eax, cg_ecx);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
//    }
    break;
  case rv_inst_and:
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
//    if (i->rs1 == i->rd) {
//      cg_and_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
//    }
//    else {
      cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
      cg_and_r32_r32(cg, cg_eax, cg_ecx);
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
//    }
    break;

  case rv_inst_fence:
    break;

  case rv_inst_ecall:
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.on_ecall));
    break;
  case rv_inst_ebreak:
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.on_ebreak));
    break;

  // RV32M
  case rv_inst_mul:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    cg_imul_r32(cg, cg_ecx);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_mulh:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    cg_imul_r32(cg, cg_ecx);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_edx);
    break;
  case rv_inst_mulhu:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, i->rs1);
    cg_mov_r32_r64disp(cg, cg_ecx, cg_rsi, i->rs2);
    cg_imul_r32(cg, cg_ecx);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_edx);
    break;
  case rv_inst_mulhsu:
  case rv_inst_div:
  case rv_inst_divu:
  case rv_inst_rem:
  case rv_inst_remu:
    // offload to a specific instruction handler
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);   // arg1 - rv
    cg_mov_r32_i32(cg, cg_edx, inst);     // arg2 - inst
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, jit.handle_op_op));
    break;

  // RV32F
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
    break;

  // RV32 Zicsr
  case rv_inst_csrrw:
  case rv_inst_csrrs:
  case rv_inst_csrrc:
  case rv_inst_csrrwi:
  case rv_inst_csrrsi:
  case rv_inst_csrrci:
    // offload to a specific instruction handler
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);   // arg1 - rv
    cg_mov_r32_i32(cg, cg_edx, inst);     // arg2 - inst
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, jit.handle_op_system));
    break;

  // RV32 Zifencei
  case rv_inst_fencei:
    break;

  // RV32A
  case rv_inst_lrw:
  case rv_inst_scw:
  case rv_inst_amoswapw:
  case rv_inst_amoaddw:
  case rv_inst_amoxorw:
  case rv_inst_amoandw:
  case rv_inst_amoorw:
  case rv_inst_amominw:
  case rv_inst_amomaxw:
  case rv_inst_amominuw:
  case rv_inst_amomaxuw:
    break;

  default:
    assert(!"unreachable");
    return false;
  }

  // success
  return true;
}
