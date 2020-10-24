#include <assert.h>
#include <stddef.h>

#include "riscv_private.h"
#include "decode.h"

#include "../tinycg/tinycg.h"


// calling convention
//
//        windows     linux
//  arg1  RCX         RDI
//  arg2  RDX         RSI
//  arg3  R8          RDX
//  arg4  R9          RCX
//  arg5              R8
//  arg6              R9
//
//  callee save
//    windows   RBX, RBP, RDI, RSI, R12, R13, R14, R15
//    linux     RBX, RBP,           R12, R13, R14, R15
//
//  caller save
//    windows   RAX, RCX, RDX,           R8, R9, R10, R11
//    linux     RAX, RCX, RDX, RDI, RSI, R8, R9, R10, R11
//
//  erata:
//    windows - caller must allocate 32bytes of shadow space.
//    windows - stack must be 16 byte aligned.
//    linux   - no shadow space needed.


// byte offset from rv structure address to member address
#define rv_offset(RV, MEMBER) ((int32_t)(((uintptr_t)&(RV.MEMBER)) - (uintptr_t)&RV))

// this struct is only used for offset calculations
static struct riscv_t rv;

static void get_reg(struct cg_state_t *cg, cg_r32_t dst, uint32_t src) {
  if (src == rv_reg_zero) {
    cg_xor_r32_r32(cg, dst, dst);
  }
  else {
    cg_mov_r32_r64disp(cg, dst, cg_rsi, rv_offset(rv, X[src]));
  }
}

static void set_reg(struct cg_state_t *cg, uint32_t dst, cg_r32_t src) {
  if (dst != rv_reg_zero) {
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[dst]), src);
  }
}

static void set_regi(struct cg_state_t *cg, uint32_t dst, int32_t imm) {
  if (dst != rv_reg_zero) {
    cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[dst]), imm);
  }
}

bool codegen(const struct rv_inst_t *i, struct cg_state_t *cg, uint32_t pc, uint32_t inst) {

  // skip instructions that would purely store to X0
  if (i->rd == rv_reg_zero) {
    // some instructions have rd == 0 but we process them anyway
    if (!inst_bypass_zero_store(i)) {
      return true;
    }
  }

  switch (i->opcode) {

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  // RV32I
  case rv_inst_lui:
    set_regi(cg, i->rd, i->imm);
    break;
  case rv_inst_auipc:
    set_regi(cg, i->rd, pc + i->imm);
    break;
  case rv_inst_jal:
    cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, PC), pc + i->imm);
    set_regi(cg, i->rd, pc + 4);
    break;
  case rv_inst_jalr:
    if (i->rs1 == rv_reg_zero) {
      cg_mov_r32_i32(cg, cg_eax, i->imm & 0xfffffffe);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      if (i->imm) {
        cg_add_r32_i32(cg, cg_eax, i->imm);
      }
      cg_and_r32_i32(cg, cg_eax, 0xfffffffe);
    }
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, PC), cg_eax);        // branch
    set_regi(cg, i->rd, pc + 4);                                      // link
    break;
  case rv_inst_beq:
  case rv_inst_bne:
  case rv_inst_blt:
  case rv_inst_bge:
  case rv_inst_bltu:
  case rv_inst_bgeu:
    get_reg(cg, cg_eax, i->rs1);
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
    if (i->rs1 == rv_reg_zero) {
      cg_mov_r32_i32(cg, cg_edx, i->imm);                               // addr
    }
    else {
      get_reg(cg, cg_edx, i->rs1);
      if (i->imm) {
        cg_add_r32_i32(cg, cg_edx, i->imm);                             // addr
      }
    }
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
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_sb:
  case rv_inst_sh:
  case rv_inst_sw:
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);                                 // rv
    if (i->rs1 == rv_reg_zero) {
      cg_mov_r32_i32(cg, cg_edx, i->imm);                               // addr
    }
    else {
      get_reg(cg, cg_edx, i->rs1);
      if (i->imm) {
        cg_add_r32_i32(cg, cg_edx, i->imm);                             // addr
      }
    }
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
      if (i->rs1 == rv_reg_zero) {
        set_regi(cg, i->rd, i->imm);
      }
      else {
        get_reg(cg, cg_eax, i->rs1);
        if (i->imm) {
          cg_add_r32_i32(cg, cg_eax, i->imm);
        }
        set_reg(cg, i->rd, cg_eax);
      }
    }
    break;
  case rv_inst_slti:
    cg_cmp_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rs1]), i->imm);
    cg_setcc_r8(cg, cg_cc_lt, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_sltiu:
    cg_cmp_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rs1]), i->imm);
    cg_setcc_r8(cg, cg_cc_c, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_xori:
    if (i->rd == i->rs1) {
      cg_xor_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_xor_r32_i32(cg, cg_eax, i->imm);
      set_reg(cg, i->rd, cg_eax);
    }
    break;
  case rv_inst_ori:
    if (i->rd == i->rs1) {
      cg_or_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_or_r32_i32(cg, cg_eax, i->imm);
      set_reg(cg, i->rd, cg_eax);
    }
    break;
  case rv_inst_andi:
    if (i->rd == i->rs1) {
      cg_and_r64disp_i32(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_and_r32_i32(cg, cg_eax, i->imm);
      set_reg(cg, i->rd, cg_eax);
    }
    break;
  case rv_inst_slli:
    if (i->rd == i->rs1) {
      cg_shl_r64disp_i8(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm & 0x1f);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_shl_r32_i8(cg, cg_eax, i->imm & 0x1f);
      set_reg(cg, i->rd, cg_eax);
    }
    break;
  case rv_inst_srli:
    if (i->rd == i->rs1) {
      cg_shr_r64disp_i8(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm & 0x1f);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_shr_r32_i8(cg, cg_eax, i->imm & 0x1f);
      set_reg(cg, i->rd, cg_eax);
    }
    break;
  case rv_inst_srai:
    if (i->rd == i->rs1) {
      cg_sar_r64disp_i8(cg, cg_rsi, rv_offset(rv, X[i->rd]), i->imm & 0x1f);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_sar_r32_i8(cg, cg_eax, i->imm & 0x1f);
      set_reg(cg, i->rd, cg_eax);
    }
    break;

  case rv_inst_add:
    if (i->rs2 == rv_reg_zero) {
      get_reg(cg, cg_eax, i->rs1);
      set_reg(cg, i->rd, cg_eax);
    }
    else {
      get_reg(cg, cg_ecx, i->rs2);
      if (i->rs1 == i->rd) {
        cg_add_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
      }
      else {
        if (i->rs1 == rv_reg_zero) {
          set_reg(cg, i->rd, cg_ecx);
        }
        else {
          get_reg(cg, cg_eax, i->rs1);
          cg_add_r32_r32(cg, cg_eax, cg_ecx);
          set_reg(cg, i->rd, cg_eax);
        }
      }
    }
    break;
  case rv_inst_sub:
    get_reg(cg, cg_ecx, i->rs2);
    if (i->rs1 == i->rd) {
      cg_sub_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_sub_r32_r32(cg, cg_eax, cg_ecx);
      set_reg(cg, i->rd, cg_eax);
    }
    break;
  case rv_inst_sll:
    get_reg(cg, cg_eax, i->rs1);
    get_reg(cg, cg_ecx, i->rs2);
    cg_and_r8_i8(cg, cg_cl, 0x1f);
    cg_shl_r32_cl(cg, cg_eax);
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_slt:
    get_reg(cg, cg_eax, i->rs1);
    cg_cmp_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, X[i->rs2]));
    cg_setcc_r8(cg, cg_cc_lt, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_sltu:
    get_reg(cg, cg_eax, i->rs1);
    cg_cmp_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, X[i->rs2]));
    cg_setcc_r8(cg, cg_cc_c, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_xor:
    get_reg(cg, cg_ecx, i->rs2);
    if (i->rs1 == i->rd) {
      cg_xor_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_xor_r32_r32(cg, cg_eax, cg_ecx);
      set_reg(cg, i->rd, cg_eax);
    }
    break;
  case rv_inst_srl:
    get_reg(cg, cg_eax, i->rs1);
    get_reg(cg, cg_ecx, i->rs2);
    cg_and_r8_i8(cg, cg_cl, 0x1f);
    cg_shr_r32_cl(cg, cg_eax);
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_sra:
    get_reg(cg, cg_eax, i->rs1);
    get_reg(cg, cg_ecx, i->rs2);
    cg_and_r8_i8(cg, cg_cl, 0x1f);
    cg_sar_r32_cl(cg, cg_eax);
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_or:
    get_reg(cg, cg_ecx, i->rs2);
    if (i->rs1 == i->rd) {
      cg_or_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_or_r32_r32(cg, cg_eax, cg_ecx);
      set_reg(cg, i->rd, cg_eax);
    }
    break;
  case rv_inst_and:
    get_reg(cg, cg_ecx, i->rs2);
    if (i->rs1 == i->rd) {
      cg_and_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_ecx);
    }
    else {
      get_reg(cg, cg_eax, i->rs1);
      cg_and_r32_r32(cg, cg_eax, cg_ecx);
      set_reg(cg, i->rd, cg_eax);
    }
    break;

  case rv_inst_fence:
    break;

  case rv_inst_ecall:
    cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, PC), pc + 4);
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.on_ecall));
    break;
  case rv_inst_ebreak:
    cg_mov_r64disp_i32(cg, cg_rsi, rv_offset(rv, PC), pc + 4);
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.on_ebreak));
    break;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  // RV32M
  case rv_inst_mul:
    get_reg(cg, cg_eax, i->rs1);
    cg_imul_r64disp(cg, cg_rsi, rv_offset(rv, X[i->rs2]));
    set_reg(cg, i->rd, cg_eax);
    break;
  case rv_inst_mulh:
    get_reg(cg, cg_eax, i->rs1);
    cg_imul_r64disp(cg, cg_rsi, rv_offset(rv, X[i->rs2]));
    set_reg(cg, i->rd, cg_edx);
    break;
  case rv_inst_mulhu:
    get_reg(cg, cg_eax, i->rs1);
    cg_mul_r64disp(cg, cg_rsi, rv_offset(rv, X[i->rs2]));
    set_reg(cg, i->rd, cg_edx);
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

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  // RV32F
  case rv_inst_flw:
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);                                 // rv
    get_reg(cg, cg_edx, i->rs1);
    if (i->imm) {
      cg_add_r32_i32(cg, cg_edx, i->imm);                               // addr
    }
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_read_w));          // read
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_eax);
    break;
  case rv_inst_fsw:
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);                                 // rv
    if (i->rs1 == rv_reg_zero) {
      cg_mov_r32_i32(cg, cg_edx, i->imm);                               // addr
    }
    else {
      get_reg(cg, cg_edx, i->rs1);
      if (i->imm) {
        cg_add_r32_i32(cg, cg_edx, i->imm);                             // addr
      }
    }
    cg_movsx_r64_r64disp(cg, cg_r8, cg_rsi, rv_offset(rv, F[i->rs2]));  // value
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, io.mem_write_w));         // write
    break;
  case rv_inst_fmadds:
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs2]));
    cg_addss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs3]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fmsubs:
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs2]));
    cg_subss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs3]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fnmsubs:
    // multiply
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs2]));
    // negate
    cg_mov_r32_xmm(cg, cg_eax, cg_xmm0);
    cg_xor_r32_i32(cg, cg_eax, 0x80000000);
    cg_mov_xmm_r32(cg, cg_xmm0, cg_eax);
    // add
    cg_addss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs3]));
    // store
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fnmadds:
    // multiply
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs2]));
    // negate
    cg_mov_r32_xmm(cg, cg_eax, cg_xmm0);
    cg_xor_r32_i32(cg, cg_eax, 0x80000000);
    cg_mov_xmm_r32(cg, cg_xmm0, cg_eax);
    // subtract
    cg_subss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs3]));
    // store
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fadds:
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_addss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs2]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fsubs:
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_subss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs2]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fmuls:
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs2]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fdivs:
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_divss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs2]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fsqrts:
    cg_sqrtss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fsgnjs:
  case rv_inst_fsgnjns:
  case rv_inst_fsgnjxs:
  case rv_inst_fmins:
  case rv_inst_fmaxs:
  case rv_inst_feqs:
  case rv_inst_flts:
  case rv_inst_fles:
  case rv_inst_fclasss:
    // defer to a handler function for these ones
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);   // arg1 - rv
    cg_mov_r32_i32(cg, cg_edx, inst);     // arg2 - inst
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, jit.handle_op_fp));
    break;
  case rv_inst_fmvxw:
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_fcvtws:
  case rv_inst_fcvtwus:
    cg_cvttss2si_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, F[i->rs1]));
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[i->rd]), cg_eax);
    break;
  case rv_inst_fcvtsw:
  case rv_inst_fcvtswu:
    cg_cvtsi2ss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, X[i->rs1]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_xmm0);
    break;
  case rv_inst_fmvwx:
    get_reg(cg, cg_eax, i->rs1);
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, F[i->rd]), cg_eax);
    break;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
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

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  // RV32 Zifencei
  case rv_inst_fencei:
    break;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
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

void codegen_prologue(struct cg_state_t *cg, bool is_leaf) {
  if (is_leaf) {
    cg_push_r64(cg, cg_rsi);
    cg_mov_r64_r64(cg, cg_rsi, cg_rcx);
  }
  else {
    // new stack frame
    cg_push_r64(cg, cg_rbp);
    cg_mov_r64_r64(cg, cg_rbp, cg_rsp);
    cg_sub_r64_i32(cg, cg_rsp, 64);
    // save rsi
    cg_mov_r64disp_r64(cg, cg_rsp, 32, cg_rsi);
    // move rv struct pointer into rsi
    cg_mov_r64_r64(cg, cg_rsi, cg_rcx);
  }
}

void codegen_epilogue(struct cg_state_t *cg, bool is_leaf) {
  if (is_leaf) {
    cg_pop_r64(cg, cg_rsi);
  }
  else {
    // restore rsi
    cg_mov_r64_r64disp(cg, cg_rsi, cg_rsp, 32);
    // leave stack frame
    cg_mov_r64_r64(cg, cg_rsp, cg_rbp);
    cg_pop_r64(cg, cg_rbp);
  }
  // return
  cg_ret(cg);
}
