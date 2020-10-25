#include <assert.h>
#include <stddef.h>

#include "riscv_private.h"
#include "decode.h"


static bool emulate(riscv_t &rv, const rv_inst_t &i) {

  // note: rv.PC is not the real PC of the instruction and should not be used
  //       except to update the PC.  To read the PC i.pc should be used.

  static const int32_t inst_size = 4;

  switch (i.opcode) {
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  // RV32I
  case rv_inst_lui:
    rv.X[i.rd] = i.imm;
    break;
  case rv_inst_auipc:
    rv.X[i.rd] = i.imm + i.pc;
    break;
  case rv_inst_jal:
    rv.PC = i.pc + i.imm;
    rv.X[i.rd] = i.pc + inst_size;
    break;
  case rv_inst_jalr:
    rv.PC = (rv.X[i.rs1] + i.imm) & ~1u;
    rv.X[i.rd] = i.pc + inst_size;
    break;
  case rv_inst_beq:
    rv.PC = i.pc + ((rv.X[i.rs1] == rv.X[i.rs2]) ? i.imm : inst_size);
    break;
  case rv_inst_bne:
    rv.PC = i.pc + ((rv.X[i.rs1] != rv.X[i.rs2]) ? i.imm : inst_size);
    break;
  case rv_inst_blt:
    rv.PC = i.pc + (((int32_t)rv.X[i.rs1] < (int32_t)rv.X[i.rs2]) ? i.imm : inst_size);
    break;
  case rv_inst_bge:
    rv.PC = i.pc + (((int32_t)rv.X[i.rs1] >= (int32_t)rv.X[i.rs2]) ? i.imm : inst_size);
    break;
  case rv_inst_bltu:
    rv.PC = i.pc + ((rv.X[i.rs1] < rv.X[i.rs2]) ? i.imm : inst_size);
    break;
  case rv_inst_bgeu:
    rv.PC = i.pc + ((rv.X[i.rs1] >= rv.X[i.rs2]) ? i.imm : inst_size);
    break;
  case rv_inst_lb:
    rv.X[i.rd] = sign_extend_b(rv.io.mem_read_b(&rv, rv.X[i.rs1] + i.imm));
    break;
  case rv_inst_lh:
    rv.X[i.rd] = sign_extend_h(rv.io.mem_read_s(&rv, rv.X[i.rs1] + i.imm));
    break;
  case rv_inst_lw:
    rv.X[i.rd] = rv.io.mem_read_w(&rv, rv.X[i.rs1] + i.imm);
    break;
  case rv_inst_lbu:
    rv.X[i.rd] = rv.io.mem_read_b(&rv, rv.X[i.rs1] + i.imm);
    break;
  case rv_inst_lhu:
    rv.X[i.rd] = rv.io.mem_read_s(&rv, rv.X[i.rs1] + i.imm);
    break;
  case rv_inst_sb:
    rv.io.mem_write_b(&rv, rv.X[i.rs1] + i.imm, rv.X[i.rs2]);
    break;
  case rv_inst_sh:
    rv.io.mem_write_s(&rv, rv.X[i.rs1] + i.imm, rv.X[i.rs2]);
    break;
  case rv_inst_sw:
    rv.io.mem_write_w(&rv, rv.X[i.rs1] + i.imm, rv.X[i.rs2]);
    break;
  case rv_inst_addi:
    rv.X[i.rd] = (int32_t)(rv.X[i.rs1]) + i.imm;
    break;
  case rv_inst_slti:
    rv.X[i.rd] = ((int32_t)(rv.X[i.rs1]) < i.imm) ? 1 : 0;
    break;
  case rv_inst_sltiu:
    rv.X[i.rd] = (rv.X[i.rs1] < (uint32_t)i.imm) ? 1 : 0;
    break;
  case rv_inst_xori:
    rv.X[i.rd] = rv.X[i.rs1] ^ i.imm;
    break;
  case rv_inst_ori:
    rv.X[i.rd] = rv.X[i.rs1] | i.imm;
    break;
  case rv_inst_andi:
    rv.X[i.rd] = rv.X[i.rs1] & i.imm;
    break;
  case rv_inst_slli:
    rv.X[i.rd] = rv.X[i.rs1] << (i.imm & 0x1f);
    break;
  case rv_inst_srli:
    rv.X[i.rd] = rv.X[i.rs1] >> (i.imm & 0x1f);
    break;
  case rv_inst_srai:
    rv.X[i.rd] = ((int32_t)rv.X[i.rs1]) >> (i.imm & 0x1f);
    break;
  case rv_inst_add:
    rv.X[i.rd] = (int32_t)(rv.X[i.rs1]) + (int32_t)(rv.X[i.rs2]);
    break;
  case rv_inst_sub:
    rv.X[i.rd] = (int32_t)(rv.X[i.rs1]) - (int32_t)(rv.X[i.rs2]);
    break;
  case rv_inst_sll:
    rv.X[i.rd] = rv.X[i.rs1] << (rv.X[i.rs2] & 0x1f);
    break;
  case rv_inst_slt:
    rv.X[i.rd] = ((int32_t)(rv.X[i.rs1]) < (int32_t)(rv.X[i.rs2])) ? 1 : 0;
    break;
  case rv_inst_sltu:
    rv.X[i.rd] = (rv.X[i.rs1] < rv.X[i.rs2]) ? 1 : 0;
    break;
  case rv_inst_xor:
    rv.X[i.rd] = rv.X[i.rs1] ^ rv.X[i.rs2];
    break;
  case rv_inst_srl:
    rv.X[i.rd] = rv.X[i.rs1] >> (rv.X[i.rs2] & 0x1f);
    break;
  case rv_inst_sra:
    rv.X[i.rd] = ((int32_t)rv.X[i.rs1]) >> (rv.X[i.rs2] & 0x1f);
    break;
  case rv_inst_or:
    rv.X[i.rd] = rv.X[i.rs1] | rv.X[i.rs2];
    break;
  case rv_inst_and:
    rv.X[i.rd] = rv.X[i.rs1] & rv.X[i.rs2];
    break;
  case rv_inst_fence:
    break;
  case rv_inst_ecall:
    rv.io.on_ecall(&rv);
    rv.PC = i.pc + 4;
    break;
  case rv_inst_ebreak:
    rv.io.on_ebreak(&rv);
    rv.PC = i.pc + 4;
    break;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  // RV32M
  case rv_inst_mul:
    rv.X[i.rd] = (int32_t)rv.X[i.rs1] * (int32_t)rv.X[i.rs2];
    break;
  case rv_inst_mulh:
    {
      const int64_t a = (int32_t)rv.X[i.rs1];
      const int64_t b = (int32_t)rv.X[i.rs2];
      rv.X[i.rd] = ((uint64_t)(a * b)) >> 32;
    }
    break;
  case rv_inst_mulhu:
    rv.X[i.rd] = ((uint64_t)rv.X[i.rs1] * (uint64_t)rv.X[i.rs2]) >> 32;
    break;
  case rv_inst_mulhsu:
    {
      const int64_t a = (int32_t)rv.X[i.rs1];
      const uint64_t b = rv.X[i.rs2];
      rv.X[i.rd] = ((uint64_t)(a * b)) >> 32;
    }
    break;
  case rv_inst_div:
    {
      const int32_t dividend = (int32_t)rv.X[i.rs1];
      const int32_t divisor = (int32_t)rv.X[i.rs2];
      if (divisor == 0) {
        rv.X[i.rd] = ~0u;
      }
      else if (divisor == -1 && rv.X[i.rs1] == 0x80000000u) {
        rv.X[i.rd] = rv.X[i.rs1];
      }
      else {
        rv.X[i.rd] = dividend / divisor;
      }
    }
    break;
  case rv_inst_divu:
    {
      const uint32_t dividend = rv.X[i.rs1];
      const uint32_t divisor = rv.X[i.rs2];
      if (divisor == 0) {
        rv.X[i.rd] = ~0u;
      }
      else {
        rv.X[i.rd] = dividend / divisor;
      }
    }
    break;
  case rv_inst_rem:
    {
      const int32_t dividend = rv.X[i.rs1];
      const int32_t divisor = rv.X[i.rs2];
      if (divisor == 0) {
        rv.X[i.rd] = dividend;
      }
      else if (divisor == -1 && rv.X[i.rs1] == 0x80000000u) {
        rv.X[i.rd] = 0;
      }
      else {
        rv.X[i.rd] = dividend % divisor;
      }
    }
    break;
  case rv_inst_remu:
    {
      const uint32_t dividend = rv.X[i.rs1];
      const uint32_t divisor = rv.X[i.rs2];
      if (divisor == 0) {
        rv.X[i.rd] = dividend;
      }
      else {
        rv.X[i.rd] = dividend % divisor;
      }
    }
    break;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  // RV32F
  case rv_inst_flw:
    {
      const uint32_t data = rv.io.mem_read_w(&rv, rv.X[i.rs1] + i.imm);
      memcpy(rv.F + i.rd, &data, 4);
    }
    break;
  case rv_inst_fsw:
    {
      uint32_t data;
      memcpy(&data, (const void*)(rv.F + i.rs2), 4);
      rv.io.mem_write_w(&rv, rv.X[i.rs1] + i.imm, data);
    }
    break;
  case rv_inst_fmadds:
    rv.F[i.rd] = rv.F[i.rs1] * rv.F[i.rs2] + rv.F[i.rs3];
    break;
  case rv_inst_fmsubs:
    rv.F[i.rd] = rv.F[i.rs1] * rv.F[i.rs2] - rv.F[i.rs3];
    break;
  case rv_inst_fnmsubs:
    rv.F[i.rd] = rv.F[i.rs3] - (rv.F[i.rs1] * rv.F[i.rs2]);
    break;
  case rv_inst_fnmadds:
    rv.F[i.rd] = -(rv.F[i.rs1] * rv.F[i.rs2]) - rv.F[i.rs3];
    break;
  case rv_inst_fadds:
    rv.F[i.rd] = rv.F[i.rs1] + rv.F[i.rs2];
    break;
  case rv_inst_fsubs:
    rv.F[i.rd] = rv.F[i.rs1] - rv.F[i.rs2];
    break;
  case rv_inst_fmuls:
    rv.F[i.rd] = rv.F[i.rs1] * rv.F[i.rs2];
    break;
  case rv_inst_fdivs:
    rv.F[i.rd] = rv.F[i.rs1] / rv.F[i.rs2];
    break;
  case rv_inst_fsqrts:
    rv.F[i.rd] = sqrtf(rv.F[i.rs1]);
    break;
  case rv_inst_fsgnjs:
    {
      uint32_t f1, f2, res;
      memcpy(&f1, rv.F + i.rs1, 4);
      memcpy(&f2, rv.F + i.rs2, 4);
      res = (f1 & ~FMASK_SIGN) | (f2 & FMASK_SIGN);
      memcpy(rv.F + i.rd, &res, 4);
    }
    break;
  case rv_inst_fsgnjns:
    {
      uint32_t f1, f2, res;
      memcpy(&f1, rv.F + i.rs1, 4);
      memcpy(&f2, rv.F + i.rs2, 4);
      res = (f1 & ~FMASK_SIGN) | (~f2 & FMASK_SIGN);
      memcpy(rv.F + i.rd, &res, 4);
    }
    break;
  case rv_inst_fsgnjxs:
    {
      uint32_t f1, f2, res;
      memcpy(&f1, rv.F + i.rs1, 4);
      memcpy(&f2, rv.F + i.rs2, 4);
      res = f1 ^ (f2 & FMASK_SIGN);
      memcpy(rv.F + i.rd, &res, 4);
    }
    break;
  case rv_inst_fmins:
    rv.F[i.rd] = fminf(rv.F[i.rs1], rv.F[i.rs2]);
    break;
  case rv_inst_fmaxs:
    rv.F[i.rd] = fmaxf(rv.F[i.rs1], rv.F[i.rs2]);
    break;
  case rv_inst_feqs:
    rv.X[i.rd] = (rv.F[i.rs1] == rv.F[i.rs2]) ? 1 : 0;
    break;
  case rv_inst_flts:
    rv.X[i.rd] = (rv.F[i.rs1] < rv.F[i.rs2]) ? 1 : 0;
    break;
  case rv_inst_fles:
    rv.X[i.rd] = (rv.F[i.rs1] <= rv.F[i.rs2]) ? 1 : 0;
    break;
  case rv_inst_fclasss:
    {
      uint32_t bits;
      memcpy(&bits, rv.F + i.rs1, 4);
      rv.X[i.rd] = calc_fclass(bits);
    }
    break;
  case rv_inst_fmvxw:
    memcpy(rv.X + i.rd, rv.F + i.rs1, 4);
    break;
  case rv_inst_fcvtws:
    rv.X[i.rd] = (int32_t)rv.F[i.rs1];
    break;
  case rv_inst_fcvtwus:
    rv.X[i.rd] = (uint32_t)rv.F[i.rs1];
    break;
  case rv_inst_fcvtsw:
    rv.F[i.rd] = (float)(int32_t)rv.X[i.rs1];
    break;
  case rv_inst_fcvtswu:
    rv.F[i.rd] = (float)(uint32_t)rv.X[i.rs1];
    break;
  case rv_inst_fmvwx:
    memcpy(rv.F + i.rd, rv.X + i.rs1, 4);
    break;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  // RV32 Zicsr
  case rv_inst_csrrw:
  case rv_inst_csrrs:
  case rv_inst_csrrc:
  case rv_inst_csrrwi:
  case rv_inst_csrrsi:
  case rv_inst_csrrci:
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

void emulate_block(riscv_t *rv, block_t &block) {
  for (auto &i : block.inst) {
    // enforce zero regiser
    rv->X[rv_reg_zero] = 0;
    // emulate an instruction
    emulate(*rv, i);
  }
}
