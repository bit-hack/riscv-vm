// ___________.__              _________   ________
// \__    ___/|__| ____ ___.__.\_   ___ \ /  _____/
//   |    |   |  |/    <   |  |/    \  \//   \  ___
//   |    |   |  |   |  \___  |\     \___\    \_\  \
//   |____|   |__|___|  / ____| \______  /\______  /
//  Tiny Code Gen X64 \/\/             \/        \/
//
//  https://github.com/bit-hack/tinycg
//

#include <assert.h>
#include <string.h>

#include "tinycg.h"

const char *cg_r64_str(cg_r32_t reg) {
  static const char *str[] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  };
  return str[reg & 0xf];
}

const char *cg_r32_str(cg_r32_t reg) {
  static const char *str[] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
  };
  return str[reg & 0x7];
}

const char *cg_r16_str(cg_r32_t reg) {
  static const char *str[] = {
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
  };
  return str[reg & 0x7];
}

const char *cg_r8_str(cg_r32_t reg) {
  static const char *str[] = {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
  };
  return str[reg & 0x7];
}

static void cg_emit_data(struct cg_state_t *cg, const void *data, size_t size) {
  assert((cg->head + size) < cg->end);
  memcpy(cg->head, data, size);
  cg->head += size;
}

static void cg_modrm(struct cg_state_t *cg, uint32_t mod, uint32_t reg, uint32_t rm) {
  const uint8_t data = ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
  cg_emit_data(cg, &data, 1);
  // if we need a sib byte
  if (mod < 3 && rm == 4) {
    // scale = 0, index = none, base = esp
    const uint8_t sib = (0 << 6) | (4 << 3) | 4;
    cg_emit_data(cg, &sib, 1);
  }
}

static void cg_rex(struct cg_state_t *cg, int w, int r, int x, int b) {
  const uint8_t rex = 0x40 | (w << 3) | (r << 2) | (x << 1) | b;
  cg_emit_data(cg, &rex, 1);
}

uint32_t cg_size(struct cg_state_t *cg) {
  return (uint32_t)(cg->head - cg->start);
}

void cg_mov_r64_r64(struct cg_state_t *cg, cg_r64_t r1, cg_r64_t r2) {
  cg_rex(cg, 1, r2 >= cg_r8, 0, r1 >= cg_r8);
  cg_emit_data(cg, "\x89", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_mov_r32_r32(struct cg_state_t *cg, cg_r32_t r1, cg_r32_t r2) {
  assert(r1 == (r1 & 0x7));
  assert(r2 == (r2 & 0x7));
  cg_emit_data(cg, "\x89", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_mov_r32_i32(struct cg_state_t *cg, cg_r32_t r1, uint32_t imm) {
  assert(r1 == (r1 & 0x7));
  const uint8_t inst = 0xb8 | (r1 & 0x7);
  cg_emit_data(cg, &inst, 1);
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_mov_r64_i32(struct cg_state_t *cg, cg_r64_t r1, int32_t imm) {
  cg_rex(cg, 1, 0, 0, r1 >= cg_r8);
  cg_emit_data(cg, "\xc7", 1);
  cg_modrm(cg, 3, 0, r1);
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_mov_r64disp_r64(struct cg_state_t *cg, cg_r64_t base, int32_t disp, cg_r64_t r1) {
  cg_rex(cg, 1, r1 >= cg_r8, 0, base >= cg_r8);
  if (disp >= -128 && disp <= 127) {
    cg_emit_data(cg, "\x89", 1);
    cg_modrm(cg, 1, r1, base);
    const int8_t disp8 = disp;
    cg_emit_data(cg, &disp8, 1);
  }
  else {
    cg_emit_data(cg, "\x89", 1);
    cg_modrm(cg, 2, r1, base);
    cg_emit_data(cg, &disp, sizeof(disp));
  }
}

void cg_mov_r64disp_i32(struct cg_state_t *cg, cg_r64_t base, int32_t disp, int32_t imm) {
  cg_emit_data(cg, "\xC7", 1);
  if (disp >= -128 && disp <= 127) {
    cg_modrm(cg, 1, 0, base);
    const int8_t disp8 = disp;
    cg_emit_data(cg, &disp8, sizeof(disp8));
  }
  else {
    cg_modrm(cg, 2, 0, base);
    cg_emit_data(cg, &disp, sizeof(disp));
  }
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_mov_r64_r64disp(struct cg_state_t *cg, cg_r64_t base, cg_r64_t r1, int32_t disp) {
  cg_rex(cg, 1, base >= cg_r8, 0, r1 >= cg_r8);
  if (disp >= -128 && disp <= 127) {
    cg_emit_data(cg, "\x8b", 1);
    cg_modrm(cg, 1, base, r1);
    const int8_t disp8 = disp;
    cg_emit_data(cg, &disp8, 1);
  }
  else {
    cg_emit_data(cg, "\x8b", 1);
    cg_modrm(cg, 2, base, r1);
    cg_emit_data(cg, &disp, sizeof(disp));
  }
}

void cg_ret(struct cg_state_t *cg) {
  cg_emit_data(cg, "\xc3", 1);
}

void cg_mov_r32_r64disp(struct cg_state_t *cg, cg_r32_t r1, cg_r64_t base, int32_t disp) {
  assert(r1   == (r1   & 0x7));
  if (disp >= -128 && disp <= 127) {
    cg_emit_data(cg, "\x8b", 1);
    cg_modrm(cg, 1, r1, base);
    const uint8_t disp8 = disp;
    cg_emit_data(cg, &disp8, 1);
  }
  else {
    cg_emit_data(cg, "\x8b", 1);
    cg_modrm(cg, 2, r1, base);
    cg_emit_data(cg, &disp, sizeof(disp));
  }
}

void cg_mov_r64disp_r32(struct cg_state_t *cg, cg_r64_t base, int32_t disp, cg_r32_t r1) {
  assert(r1   == (r1   & 0x7));
  if (disp >= -128 && disp <= 127) {
    cg_emit_data(cg, "\x89", 1);
    cg_modrm(cg, 1, r1, base);
    const uint8_t disp8 = disp;
    cg_emit_data(cg, &disp8, 1);
  }
  else {
    cg_emit_data(cg, "\x89", 1);
    cg_modrm(cg, 2, r1, base);
    cg_emit_data(cg, &disp, sizeof(disp));
  }
}

void cg_movsx_r32_r8(struct cg_state_t *cg, cg_r32_t r1, cg_r8_t r2) {
  cg_emit_data(cg, "\x0f\xbe", 2);
  cg_modrm(cg, 3, r1, r2);
}

void cg_movsx_r32_r16(struct cg_state_t *cg, cg_r32_t r1, cg_r16_t r2) {
  cg_emit_data(cg, "\x0f\xbf", 2);
  cg_modrm(cg, 3, r1, r2);
}

void cg_movsx_r64_r32(struct cg_state_t *cg, cg_r64_t dst, cg_r32_t src) {
  assert(src == (src & 0x7));
  cg_rex(cg, 1, dst >= cg_r8, 0, 0);
  cg_emit_data(cg, "\x63", 1);
  cg_modrm(cg, 3, dst, src);
}

void cg_movsx_r64_r64disp(struct cg_state_t *cg, cg_r32_t dst, cg_r64_t base, int32_t disp) {
  cg_rex(cg, 1, dst >= cg_r8, 0, base >= cg_r8);
  if (disp >= -128 && disp <= 127) {
    cg_emit_data(cg, "\x63", 1);
    cg_modrm(cg, 1, dst, base);
    const int8_t disp8 = disp;
    cg_emit_data(cg, &disp8, sizeof(disp8));
  }
  else {
    cg_emit_data(cg, "\x63", 1);
    cg_modrm(cg, 2, dst, base);
    cg_emit_data(cg, &disp, sizeof(disp));
  }
}

void cg_movzx_r32_r8(struct cg_state_t *cg, cg_r32_t r1, cg_r8_t r2) {
  cg_emit_data(cg, "\x0f\xb6", 2);
  cg_modrm(cg, 3, r1, r2);
}

void cg_movzx_r32_r16(struct cg_state_t *cg, cg_r32_t r1, cg_r16_t r2) {
  cg_emit_data(cg, "\x0f\xb7", 2);
  cg_modrm(cg, 3, r1, r2);
}

void cg_add_r64_i32(struct cg_state_t *cg, cg_r64_t r1, int32_t imm) {
  if (imm == 0) {
    return;
  }
  cg_rex(cg, 1, 0, 0, r1 >= cg_r8);
  cg_add_r32_i32(cg, r1, imm);
}

void cg_add_r32_i32(struct cg_state_t *cg, cg_r32_t r1, int32_t imm) {
  if (imm == 0) {
    return;
  }
  if (imm >= -128 && imm <= 127) {
    cg_emit_data(cg, "\x83", 1);
    cg_modrm(cg, 3, 0, r1);
    const int8_t imm8 = imm;
    cg_emit_data(cg, &imm8, 1);
  }
  else {
    if (r1 == cg_eax) {
      cg_emit_data(cg, "\x05", 1);
    }
    else {
      cg_emit_data(cg, "\x81", 1);
      cg_modrm(cg, 3, 0, r1);
    }
    cg_emit_data(cg, &imm, sizeof(imm));
  }
}

void cg_add_r32_r32(struct cg_state_t *cg, cg_r32_t r1, cg_r32_t r2) {
  cg_emit_data(cg, "\x01", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_and_r8_i8(struct cg_state_t *cg, cg_r8_t r1, uint8_t imm) {
  if (imm == 0xff) {
    return;
  }
  if (r1 == cg_al) {
    cg_emit_data(cg, "\x24", 1);
  }
  else {
    cg_emit_data(cg, "\x80", 1);
    cg_modrm(cg, 3, 4, r1);
  }
  cg_emit_data(cg, &imm, 1);
}

void cg_and_r32_i32(struct cg_state_t *cg, cg_r32_t r1, uint32_t imm) {
  if (imm == ~0u) {
    return;
  }
  if (r1 == cg_eax) {
    cg_emit_data(cg, "\x25", 1);
  }
  else {
    cg_emit_data(cg, "\x81", 1);
    cg_modrm(cg, 3, 4, r1);
  }
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_and_r32_r32(struct cg_state_t *cg, cg_r32_t r1, cg_r32_t r2) {
  cg_emit_data(cg, "\x21", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_sub_r64_i32(struct cg_state_t *cg, cg_r64_t r1, int32_t imm) {
  if (imm == 0) {
    return;
  }
  cg_rex(cg, 1, 0, 0, r1 >= cg_r8);
  cg_sub_r32_i32(cg, r1, imm);
}

void cg_sub_r32_i32(struct cg_state_t *cg, cg_r32_t r1, int32_t imm) {
  if (imm == 0) {
    return;
  }
  if (imm >= -128 && imm <= 127) {
    cg_emit_data(cg, "\x83", 1);
    const uint8_t op = 0xe8 | (r1 & 0x7);
    cg_emit_data(cg, &op, 1);
    const int8_t imm8 = imm;
    cg_emit_data(cg, &imm8, 1);
  }
  else {
    if (r1 == cg_eax) {
      cg_emit_data(cg, "\x2d", 1);
    }
    else {
      cg_emit_data(cg, "\x81", 1);
      cg_modrm(cg, 3, 5, r1);
    }
    cg_emit_data(cg, &imm, sizeof(imm));
  }
}

void cg_sub_r32_r32(struct cg_state_t *cg, cg_r32_t r1, cg_r32_t r2) {
  cg_emit_data(cg, "\x29", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_shl_r32_i8(struct cg_state_t *cg, cg_r32_t r1, uint8_t imm) {
  if (imm == 0) {
    return;
  }
  if (imm == 1) {
    const uint8_t op = 0xe0 | (r1 & 0x7);
    cg_emit_data(cg, "\xd1", 1);
    cg_emit_data(cg, &op, 1);
  }
  else {
    cg_emit_data(cg, "\xc1", 1);
    cg_modrm(cg, 3, 4, r1);
    cg_emit_data(cg, &imm, sizeof(imm));
  }
}

void cg_shl_r32_cl(struct cg_state_t *cg, cg_r32_t r1) {
  cg_emit_data(cg, "\xd3", 1);
  cg_modrm(cg, 3, 4, r1);
}

void cg_sar_r32_i8(struct cg_state_t *cg, cg_r32_t r1, uint8_t imm) {
  if (imm == 0) {
    return;
  }
  if (imm == 1) {
    const uint8_t op = 0xf8 | (r1 & 0x7);
    cg_emit_data(cg, "\xd1", 1);
    cg_emit_data(cg, &op, 1);
  }
  else {
    cg_emit_data(cg, "\xc1", 1);
    cg_modrm(cg, 3, 7, r1);
    cg_emit_data(cg, &imm, 1);
  }
}

void cg_sar_r32_cl(struct cg_state_t *cg, cg_r32_t r1) {
  cg_emit_data(cg, "\xd3", 1);
  cg_modrm(cg, 3, 7, r1);
}

void cg_shr_r32_i8(struct cg_state_t *cg, cg_r32_t r1, uint8_t imm) {
  if (imm == 0) {
    return;
  }
  if (imm == 1) {
    const uint8_t op = 0xe8 | (r1 & 0x7);
    cg_emit_data(cg, "\xd1", 1);
    cg_emit_data(cg, &op, 1);
  }
  else {
    cg_emit_data(cg, "\xc1", 1);
    cg_modrm(cg, 3, 5, r1);
    cg_emit_data(cg, &imm, sizeof(imm));
  }
}

void cg_shr_r32_cl(struct cg_state_t *cg, cg_r32_t r1) {
  cg_emit_data(cg, "\xd3", 1);
  cg_modrm(cg, 3, 5, r1);
}

void cg_xor_r32_i32(struct cg_state_t *cg, cg_r32_t r1, uint32_t imm) {
  if (imm == 0) {
    return;
  }
  if (r1 == cg_eax) {
    cg_emit_data(cg, "\x35", 1);
  }
  else {
    cg_emit_data(cg, "\x81", 1);
    cg_modrm(cg, 3, 6, r1);
  }
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_xor_r32_r32(struct cg_state_t *cg, cg_r32_t r1, cg_r32_t r2) {
  cg_emit_data(cg, "\x31", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_xor_r64_r64(struct cg_state_t *cg, cg_r64_t r1, cg_r64_t r2) {
  cg_rex(cg, 1, r2 >= cg_r8, 0, r1 >= cg_r8);
  cg_emit_data(cg, "\x31", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_or_r32_i32(struct cg_state_t *cg, cg_r32_t r1, uint32_t imm) {
  if (imm == 0) {
    return;
  }
  if (r1 == cg_eax) {
    cg_emit_data(cg, "\x0d", 1);
  }
  else {
    cg_emit_data(cg, "\x81", 1);
    cg_modrm(cg, 3, 1, r1);
  }
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_or_r32_r32(struct cg_state_t *cg, cg_r32_t r1, cg_r32_t r2) {
  cg_emit_data(cg, "\x09", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_cmp_r64_r64(struct cg_state_t *cg, cg_r64_t r1, cg_r64_t r2) {
  cg_rex(cg, 1, r2 >= cg_r8, 0, r1 >= cg_r8);
  cg_emit_data(cg, "\x39", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_cmp_r32_r32(struct cg_state_t *cg, cg_r32_t r1, cg_r32_t r2) {
  cg_emit_data(cg, "\x39", 1);
  cg_modrm(cg, 3, r2, r1);
}

void cg_cmp_r32_i32(struct cg_state_t *cg, cg_r32_t r1, uint32_t imm) {
  if (r1 == cg_eax) {
    cg_emit_data(cg, "\x3d", 1);
  }
  else {
    cg_emit_data(cg, "\x81", 1);
    cg_modrm(cg, 3, 7, r1);
  }
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_call_r64disp(struct cg_state_t *cg, cg_r64_t base, int32_t disp) {
  assert(base == (base & 0x7));
  if (disp >= -128 && disp <= 127) {
    cg_emit_data(cg, "\xff", 1);
    const uint8_t op = 0x50 | (base & 0x7);
    cg_emit_data(cg, &op, 1);
    const int8_t disp8 = disp;
    cg_emit_data(cg, &disp8, 1);
  }
  else {
    cg_emit_data(cg, "\xff", 1);
    const uint8_t op = 0x90 | (base & 0x7);
    cg_emit_data(cg, &op, 1);
    cg_emit_data(cg, &disp, sizeof(disp));
  }
}

void cg_mul_r32(struct cg_state_t *cg, cg_r32_t r1) {
  cg_emit_data(cg, "\xF7", 1);
  cg_modrm(cg, 3, 4, r1);
}

void cg_imul_r32(struct cg_state_t *cg, cg_r32_t r1) {
  cg_emit_data(cg, "\xF7", 1);
  cg_modrm(cg, 3, 5, r1);
}

void cg_push_r64(struct cg_state_t *cg, cg_r64_t r1) {
  assert(r1 == (r1 & 0x7));
  const uint8_t inst = 0x50 | (r1 & 0x7);
  cg_emit_data(cg, &inst, 1);
}

void cg_pop_r64(struct cg_state_t *cg, cg_r64_t r1) {
  assert(r1 == (r1 & 0x7));
  const uint8_t inst = 0x58 | (r1 & 0x7);
  cg_emit_data(cg, &inst, 1);
}

void cg_nop(struct cg_state_t *cg) {
  cg_emit_data(cg, "\x90", 1);
}

void cg_setcc_r8(struct cg_state_t *cg, cg_cc_t cc, cg_r8_t r1) {
  cg_emit_data(cg, "\x0f", 1);
  const uint8_t op = 0x90 | (cc & 0xf);
  cg_emit_data(cg, &op, 1);
  cg_modrm(cg, 3, 0, r1);
}

void cg_cmov_r32_r32(struct cg_state_t *cg, cg_cc_t cc, cg_r32_t r1, cg_r32_t r2) {
  cg_emit_data(cg, "\x0f", 1);
  const uint8_t op = 0x40 | (cc & 0xf);
  cg_emit_data(cg, &op, 1);
  cg_modrm(cg, 3, r1, r2);
}

void cg_reset(struct cg_state_t *cg) {
  cg->head = cg->start;
}

void cg_init(struct cg_state_t *cg, uint8_t *start, uint8_t *end) {
  cg->start = start;
  cg->head = start;
  cg->end = end;
  memset(start, 0xcc, end - start);
}

void cg_movss_xmm_r64disp(struct cg_state_t *cg, cg_xmm_t dst, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf3\x0f\x10", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_movss_r64disp_xmm(struct cg_state_t *cg, cg_r64_t base, int32_t offset, cg_xmm_t dst) {
  cg_emit_data(cg, "\xf3\x0f\x11", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_addss_xmm_r64disp(struct cg_state_t *cg, cg_xmm_t dst, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf3\x0f\x58", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_subss_xmm_r64disp(struct cg_state_t *cg, cg_xmm_t dst, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf3\x0f\x5C", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_mulss_xmm_r64disp(struct cg_state_t *cg, cg_xmm_t dst, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf3\x0f\x59", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_divss_xmm_r64disp(struct cg_state_t *cg, cg_xmm_t dst, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf3\x0f\x5E", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_sqrtss_xmm_r64disp(struct cg_state_t *cg, cg_xmm_t dst, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf3\x0f\x51", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_cvttss2si_r32_r64disp(struct cg_state_t *cg, cg_r32_t dst, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf3\x0f\x2C", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_cvtsi2ss_xmm_r64disp(struct cg_state_t *cg, cg_xmm_t dst, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf3\x0f\x2A", 3);
  cg_modrm(cg, 2, dst, base);
  cg_emit_data(cg, &offset, sizeof(offset));
}

void cg_mov_r32_xmm(struct cg_state_t *cg, cg_r32_t dst, cg_xmm_t src) {
  cg_emit_data(cg, "\x66\x0F\x7E", 3);
  cg_modrm(cg, 3, src, dst);
}

void cg_mov_xmm_r32(struct cg_state_t *cg, cg_xmm_t dst, cg_r32_t src) {
  cg_emit_data(cg, "\x66\x0F\x6E", 3);
  cg_modrm(cg, 3, dst, src);
}

static void cg_sub_r64disp_i32_generic(struct cg_state_t *cg, uint8_t op, cg_r64_t base, int32_t offset, int32_t imm) {
  assert(base == (base & 7));
  if (imm >= -128 && imm <= 127) {
    cg_emit_data(cg, "\x83", 1);
  }
  else {
    cg_emit_data(cg, "\x81", 1);
  }
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, op, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, op, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
  if (imm >= -128 && imm <= 127) {
    const int8_t imm8 = imm;
    cg_emit_data(cg, &imm8, sizeof(imm8));
  }
  else {
    cg_emit_data(cg, &imm, sizeof(imm));
  }
}

void cg_add_r64disp_i32(struct cg_state_t *cg, cg_r64_t base, int32_t offset, int32_t imm) {
  cg_sub_r64disp_i32_generic(cg, 0, base, offset, imm);
}

void cg_sub_r64disp_i32(struct cg_state_t *cg, cg_r64_t base, int32_t offset, int32_t imm) {
  cg_sub_r64disp_i32_generic(cg, 5, base, offset, imm);
}

void cg_and_r64disp_i32(struct cg_state_t *cg, cg_r64_t base, int32_t offset, int32_t imm) {
  cg_sub_r64disp_i32_generic(cg, 4, base, offset, imm);
}

void cg_or_r64disp_i32(struct cg_state_t *cg, cg_r64_t base, int32_t offset, int32_t imm) {
  cg_sub_r64disp_i32_generic(cg, 1, base, offset, imm);
}

void cg_xor_r64disp_i32(struct cg_state_t *cg, cg_r64_t base, int32_t offset, int32_t imm) {
  cg_sub_r64disp_i32_generic(cg, 6, base, offset, imm);
}

void cg_shl_r64disp_i8(struct cg_state_t *cg, cg_r64_t base, int32_t offset, uint8_t imm) {
  cg_emit_data(cg, "\xc1", 1);
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, 4, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, 4, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_shr_r64disp_i8(struct cg_state_t *cg, cg_r64_t base, int32_t offset, uint8_t imm) {
  cg_emit_data(cg, "\xc1", 1);
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, 5, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, 5, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_sar_r64disp_i8(struct cg_state_t *cg, cg_r64_t base, int32_t offset, uint8_t imm) {
  cg_emit_data(cg, "\xc1", 1);
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, 7, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, 7, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
  cg_emit_data(cg, &imm, sizeof(imm));
}

void cg_cmp_r32_r64disp(struct cg_state_t *cg, cg_r32_t r1, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\x3b", 1);
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, r1, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, r1, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
}

void cg_cmp_r64disp_r32(struct cg_state_t *cg, cg_r64_t base, int32_t offset, cg_r32_t r1) {
  cg_emit_data(cg, "\x39", 1);
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, r1, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, r1, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
}

void cg_add_r64disp_r32(struct cg_state_t *cg, cg_r64_t base, int32_t offset, cg_r32_t src) {
  cg_emit_data(cg, "\x01", 1);
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, src, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, src, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
}

void cg_mul_r64disp(struct cg_state_t *cg, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf7", 1);
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, 4, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, 4, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
}

void cg_imul_r64disp(struct cg_state_t *cg, cg_r64_t base, int32_t offset) {
  cg_emit_data(cg, "\xf7", 1);
  if (offset >= -128 && offset <= 127) {
    cg_modrm(cg, 1, 5, base);
    const int8_t offset8 = offset;
    cg_emit_data(cg, &offset8, sizeof(offset8));
  }
  else {
    cg_modrm(cg, 2, 5, base);
    cg_emit_data(cg, &offset, sizeof(offset));
  }
}
