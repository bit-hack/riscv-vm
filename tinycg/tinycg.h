// ___________.__              _________   ________
// \__    ___/|__| ____ ___.__.\_   ___ \ /  _____/
//   |    |   |  |/    <   |  |/    \  \//   \  ___
//   |    |   |  |   |  \___  |\     \___\    \_\  \
//   |____|   |__|___|  / ____| \______  /\______  /
//  Tiny Code Gen X64 \/\/             \/        \/
//
//  https://github.com/bit-hack/tinycg
//

#pragma once
#include <stdint.h>


typedef int cg_r8_t;
typedef int cg_r16_t;
typedef int cg_r32_t;
typedef int cg_r64_t;
typedef int cg_cc_t;

enum {
  cg_al,
  cg_cl,
  cg_dl,
  cg_bl,
  cg_ah,
  cg_ch,
  cg_dh,
  cg_bh,
};

enum {
  cg_ax,
  cg_cx,
  cg_dx,
  cg_bx,
  cg_sp,
  cg_bp,
  cg_si,
  cg_di,
};

enum {
  cg_eax,
  cg_ecx,
  cg_edx,
  cg_ebx,
  cg_esp,
  cg_ebp,
  cg_esi,
  cg_edi,
};

enum {
  cg_rax,
  cg_rcx,
  cg_rdx,
  cg_rbx,
  cg_rsp,
  cg_rbp,
  cg_rsi,
  cg_rdi,
  // extended registers
  cg_r8,
  cg_r9,
  cg_r10,
  cg_r11,
  cg_r12,
  cg_r13,
  cg_r14,
  cg_r15,
};

enum cc_t {
  cg_cc_o  = 0x0, // overflow         JO    (OF=1)
  cg_cc_no = 0x1, // not overflow     JNO   (OF=0)
  cg_cc_c  = 0x2, // carry            JC    (CF=1)
  cg_cc_ae = 0x3, // above or equal   JAE   (CF=0)
  cg_cc_eq = 0x4, // equal            JE    (ZF=1)
  cg_cc_ne = 0x5, // not equal        JNE   (ZF=0)
  cg_cc_be = 0x6, // below or equal   JBE   (CF=1 or ZF=1)
  cg_cc_ab = 0x7, // above            JA    (CF=0 and ZF=0)
  cg_cc_s  = 0x8, // sign             JS    (SF=1)
  cg_cc_ns = 0x9, // not sign         JNS   (SF=0)
  cg_cc_p  = 0xa, // parity           JP    (PF=1)
  cg_cc_np = 0xb, // parity odd       JNP   (PF=0)
  cg_cc_lt = 0xc, // less             JL    (SF!=OF)
  cg_cc_ge = 0xd, // greater or equal JGE   (SF=OF)
  cg_cc_le = 0xe, // less or equal    JLE   (ZF=1 or SF!=OF)
  cg_cc_gt = 0xf, // greater          JG    (ZF=0 and SF=OF)
};

struct cg_state_t {
  // the start address of the buffer
  uint8_t *start;
  // the end address of the buffer
  const uint8_t *end;
  // the next writing location in the buffer
  uint8_t *head;
};

// initalize the code generator
void cg_init(struct cg_state_t *, uint8_t *start, uint8_t *end);

// return number of bytes written to the code buffer
uint32_t cg_size(struct cg_state_t *);

// clear all bytes written to the code buffer
void cg_reset(struct cg_state_t *);

void cg_mov_r64_r64(struct cg_state_t *, cg_r32_t r1, cg_r32_t r2);
void cg_mov_r32_r32(struct cg_state_t *, cg_r32_t r1, cg_r32_t r2);
void cg_mov_r64_i32(struct cg_state_t *, cg_r32_t r1, int32_t imm);
void cg_mov_r32_i32(struct cg_state_t *, cg_r32_t r1, uint32_t imm);

void cg_ret(struct cg_state_t *);

void cg_mov_r32_r64disp(struct cg_state_t *, cg_r32_t r1, cg_r64_t base,
                        int32_t disp);
void cg_mov_r64disp_r32(struct cg_state_t *, cg_r64_t base, int32_t disp,
                        cg_r32_t r1);
void cg_mov_r64_r64disp(struct cg_state_t *cg, cg_r64_t base, cg_r64_t r1,
                        int32_t disp);
void cg_mov_r64disp_r64(struct cg_state_t *, cg_r64_t base, int32_t disp,
                        cg_r64_t r1);

void cg_movsx_r32_r8(struct cg_state_t *, cg_r32_t r1, cg_r8_t r2);
void cg_movsx_r32_r16(struct cg_state_t *, cg_r32_t r1, cg_r16_t r2);

void cg_movzx_r32_r8(struct cg_state_t *, cg_r32_t r1, cg_r8_t r2);
void cg_movzx_r32_r16(struct cg_state_t *, cg_r32_t r1, cg_r16_t r2);

void cg_add_r64_i32(struct cg_state_t *, cg_r64_t t1, int32_t imm);
void cg_add_r32_i32(struct cg_state_t *, cg_r32_t r1, int32_t imm);
void cg_add_r32_r32(struct cg_state_t *, cg_r32_t r1, cg_r32_t r2);

void cg_and_r8_i8(struct cg_state_t *, cg_r8_t r1, uint8_t imm);
void cg_and_r32_i32(struct cg_state_t *, cg_r32_t r1, uint32_t imm);
void cg_and_r32_r32(struct cg_state_t *, cg_r32_t r1, cg_r32_t r2);

void cg_sub_r64_i32(struct cg_state_t *, cg_r64_t r1, int32_t imm);
void cg_sub_r32_i32(struct cg_state_t *, cg_r32_t r1, int32_t imm);
void cg_sub_r32_r32(struct cg_state_t *, cg_r32_t r1, cg_r32_t r2);

void cg_shl_r32_i8(struct cg_state_t *, cg_r32_t r1, uint8_t imm);
void cg_shl_r32_cl(struct cg_state_t *, cg_r32_t r1);

void cg_sar_r32_i8(struct cg_state_t *, cg_r32_t r1, uint8_t imm);
void cg_sar_r32_cl(struct cg_state_t *, cg_r32_t r1);

void cg_shr_r32_i8(struct cg_state_t *, cg_r32_t r1, uint8_t imm);
void cg_shr_r32_cl(struct cg_state_t *, cg_r32_t r1);

void cg_xor_r64_r64(struct cg_state_t *, cg_r64_t r1, cg_r64_t r2);
void cg_xor_r32_i32(struct cg_state_t *, cg_r32_t r1, uint32_t imm);
void cg_xor_r32_r32(struct cg_state_t *, cg_r32_t r1, cg_r32_t r2);

void cg_or_r32_i32(struct cg_state_t *, cg_r32_t r1, uint32_t imm);
void cg_or_r32_r32(struct cg_state_t *, cg_r32_t r1, cg_r32_t r2);
void cg_setcc_r8(struct cg_state_t *, cg_cc_t cc, cg_r8_t r1);

void cg_cmp_r64_r64(struct cg_state_t *, cg_r64_t r1, cg_r64_t r2);
void cg_cmp_r32_r32(struct cg_state_t *, cg_r32_t r1, cg_r32_t r2);
void cg_cmp_r32_i32(struct cg_state_t *, cg_r32_t r1, uint32_t imm);

void cg_call_r64disp(struct cg_state_t *, cg_r64_t base, int32_t disp32);

void cg_mul_r32(struct cg_state_t *, cg_r32_t r1);
void cg_imul_r32(struct cg_state_t *, cg_r32_t r1);

void cg_push_r64(struct cg_state_t *, cg_r64_t r1);
void cg_pop_r64(struct cg_state_t *, cg_r64_t r1);

void cg_nop(struct cg_state_t *);

void cg_cmov_r32_r32(struct cg_state_t *, cg_cc_t cc, cg_r32_t r1, cg_r32_t r2);

const char *cg_r64_str(cg_r32_t reg);
const char *cg_r32_str(cg_r32_t reg);
const char *cg_r16_str(cg_r32_t reg);
const char *cg_r8_str(cg_r32_t reg);
