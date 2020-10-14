#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#if __linux__
//#include <asm/cachectl.h>
#include <sys/mman.h>
#endif

#include "riscv.h"
#include "riscv_private.h"

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


// total size of the code block
static const uint32_t code_size = 1024 * 1024 * 8;

// total number of block map entries
static const uint32_t map_size = 1024 * 64;


// flush the instruction cache for a region
static void sys_flush_icache(const void *start, size_t size) {
#ifdef _WIN32
  if (!FlushInstructionCache(GetCurrentProcess(), start, size)) {
    // failed
  }
#endif
#ifdef __linux__
//  if (cacheflush(start, size, ICACHE) == -1) {
//    // failed
//  }
#endif
}

// allocate system executable memory
static void *sys_alloc_exec_mem(uint32_t size) {
#ifdef _WIN32
  return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#endif
#ifdef __linux__
  const int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
  // mmap(addr, length, prot, flags, fd, offset)
  return mmap(NULL, size, prot, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
}

// byte offset from rv structure address to member address
#define rv_offset(RV, MEMBER) ((int32_t)(((uintptr_t)&(RV->MEMBER)) - (uintptr_t)RV))

static void set_pc(struct block_t *block, struct riscv_t *rv, cg_r32_t reg) {

  struct cg_state_t *cg = &block->cg;

  const int32_t offset = rv_offset(rv, PC);
  cg_mov_r64disp_r32(cg, cg_rsi, offset, reg);
}

static void get_pc(struct block_t *block, struct riscv_t *rv, cg_r32_t reg) {

  struct cg_state_t *cg = &block->cg;

  const int32_t offset = rv_offset(rv, PC);
  cg_mov_r32_r64disp(cg, reg, cg_rsi, offset);
}

static void get_reg(struct block_t *block, struct riscv_t *rv, cg_r32_t dst, uint32_t src) {

  struct cg_state_t *cg = &block->cg;

  if (src == rv_reg_zero) {
    cg_xor_r32_r32(cg, dst, dst);
  }
  else {
    const int32_t offset = rv_offset(rv, X[src]);
    cg_mov_r32_r64disp(cg, dst, cg_rsi, offset);
  }
}

static void get_freg(struct block_t *block, struct riscv_t *rv, cg_r32_t dst, uint32_t src) {

  struct cg_state_t *cg = &block->cg;
  const int32_t offset = rv_offset(rv, F[src]);
  cg_mov_r32_r64disp(cg, dst, cg_rsi, offset);
}

static void set_reg(struct block_t *block, struct riscv_t *rv, uint32_t dst, cg_r32_t src) {

  struct cg_state_t *cg = &block->cg;

  if (dst != rv_reg_zero) {
    const int32_t offset = rv_offset(rv, X[dst]);
    cg_mov_r64disp_r32(cg, cg_rsi, offset, src);
  }
}

static void set_freg(struct block_t *block, struct riscv_t *rv, uint32_t dst, cg_r32_t src) {

  struct cg_state_t *cg = &block->cg;

  const int32_t offset = rv_offset(rv, F[dst]);
  cg_mov_r64disp_r32(cg, cg_rsi, offset, src);
}

static void gen_prologue(struct block_t *block, struct riscv_t *rv) {
  struct cg_state_t *cg = &block->cg;
  // new stack frame
  cg_push_r64(cg, cg_rbp);
  cg_mov_r64_r64(cg, cg_rbp, cg_rsp);
  cg_sub_r64_i32(cg, cg_rsp, 64);
  // save rsi
  cg_mov_r64disp_r64(cg, cg_rsp, 32, cg_rsi);
  // move rv struct pointer into rsi
  cg_mov_r64_r64(cg, cg_rsi, cg_rcx);
}

static void gen_epilogue(struct block_t *block, struct riscv_t *rv) {
  struct cg_state_t *cg = &block->cg;
  // restore rsi
  cg_mov_r64_r64disp(cg, cg_rsi, cg_rsp, 32);
  // leave stack frame
  cg_mov_r64_r64(cg, cg_rsp, cg_rbp);
  cg_pop_r64(cg, cg_rbp);
}

// a hash function is used when mapping addresses to indexes in the block map
static uint32_t wang_hash(uint32_t a) {
  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * 0x27d4eb2d;
  a = a ^ (a >> 15);
  return a;
}

// allocate a new code block
static struct block_t *block_alloc(struct riscv_jit_t *jit) {
  // place a new block
  struct block_t *block = (struct block_t *)jit->head;
  struct cg_state_t *cg = &block->cg;
  // set the initial codegen write head
  cg_init(cg, block->code, jit->end);
  block->predict = NULL;
  return block;
}

// dump the code from a block
static void block_dump(struct block_t *block, FILE *fd) {
  fprintf(fd, "// %08x\n", block->pc_start);
  const uint8_t *ptr = block->cg.start;
  for (int i = 0; ptr + i < block->cg.head; ++i) {
    fprintf(fd, (i && i % 16 == 0) ? "\n%02x " : "%02x ", (int)ptr[i]);
  }
  fprintf(fd, "\n");
}

// finialize a code block and insert into the block map
static void block_finish(struct riscv_jit_t *jit, struct block_t *block) {
  assert(jit && block && jit->head && jit->block_map);
  struct cg_state_t *cg = &block->cg;
  // advance the block head ready for the next alloc
  jit->head = block->code + cg_size(cg);
  // insert into the block map
  uint32_t index = wang_hash(block->pc_start);
  const uint32_t mask = jit->block_map_size - 1;
  for (;; ++index) {
    if (jit->block_map[index & mask] == NULL) {
      jit->block_map[index & mask] = block;
      break;
    }
  }
#if RISCV_DUMP_JIT_TRACE
  block_dump(block, stdout);
#endif
  // flush the instructon cache for this block
  sys_flush_icache(block->code, cg_size(cg));
}

// try to locate an already translated block in the block map
static struct block_t *block_find(struct riscv_jit_t *jit, uint32_t addr) {
  assert(jit && jit->block_map);
  uint32_t index = wang_hash(addr);
  const uint32_t mask = jit->block_map_size - 1;
  for (;; ++index) {
    struct block_t *block = jit->block_map[index & mask];
    if (block == NULL) {
      return NULL;
    }
    if (block->pc_start == addr) {
      return block;
    }
  }
}

static bool op_load(struct riscv_t *rv, uint32_t inst, struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // itype format
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rd     = dec_rd(inst);

  // skip writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    block->pc_end += 4;
    block->instructions += 1;
    return true;
  }

  // arg1 - rv
  cg_mov_r64_r64(cg, cg_rcx, cg_rsi);
  // arg2 - address
  get_reg(block, rv, cg_edx, rs1);
  cg_add_r32_i32(cg, cg_edx, imm);

  int32_t offset;

  // dispatch by read size
  switch (funct3) {
  case 0: // LB
    offset = rv_offset(rv, io.mem_read_b);
    cg_call_r64disp(cg, cg_rsi, offset);
    cg_movsx_r32_r8(cg, cg_eax, cg_al);
    break;
  case 1: // LH
    offset = rv_offset(rv, io.mem_read_s);
    cg_call_r64disp(cg, cg_rsi, offset);
    cg_movsx_r32_r16(cg, cg_eax, cg_ax);
    break;
  case 2: // LW
    offset = rv_offset(rv, io.mem_read_w);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  case 4: // LBU
    offset = rv_offset(rv, io.mem_read_b);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  case 5: // LHU
    offset = rv_offset(rv, io.mem_read_s);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = rax
  set_reg(block, rv, rd, cg_eax);
  // step over instruction
  block->pc_end += 4;
  block->instructions += 1;
  // cant branch
  return true;
}

static bool op_op_imm(struct riscv_t *rv,
                      uint32_t inst,
                      struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t funct3 = dec_funct3(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    block->pc_end += 4;
    block->instructions += 1;
    return true;
  }

  // eax = rv->X[rs1]
  get_reg(block, rv, cg_eax, rs1);

  // dispatch operation type
  switch (funct3) {
  case 0: // ADDI
    cg_add_r32_i32(cg, cg_eax, imm);
    break;
  case 1: // SLLI
    cg_shl_r32_i8(cg, cg_eax, imm & 0x1f);
    break;
  case 2: // SLTI
    cg_cmp_r32_i32(cg, cg_eax, imm);
    cg_setcc_r8(cg, cg_cc_lt, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    break;
  case 3: // SLTIU
    cg_cmp_r32_i32(cg, cg_eax, imm);
    cg_setcc_r8(cg, cg_cc_c, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    break;
  case 4: // XORI
    cg_xor_r32_i32(cg, cg_eax, imm);
    break;
  case 5:
    if (imm & ~0x1f) {
      // SRAI
      cg_sar_r32_i8(cg, cg_eax, imm & 0x1f);
    }
    else {
      // SRLI
      cg_shr_r32_i8(cg, cg_eax, imm & 0x1f);
    }
    break;
  case 6: // ORI
    cg_or_r32_i32(cg, cg_eax, imm);
    break;
  case 7: // ANDI
    cg_and_r32_i32(cg, cg_eax, imm);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = eax
  set_reg(block, rv, rd, cg_eax);
  // step over instruction
  block->pc_end += 4;
  block->instructions += 1;
  // cant branch
  return true;
}

// add upper immediate to pc
static bool op_auipc(struct riscv_t *rv,
                     uint32_t inst,
                     struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // the effective current PC
  const uint32_t pc = block->pc_end;
  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t imm = dec_utype_imm(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    block->pc_end += 4;
    block->instructions += 1;
    return true;
  }

  // rv->X[rd] = imm + rv->PC;
  cg_mov_r32_i32(cg, cg_eax, pc + imm);
  set_reg(block, rv, rd, cg_eax);

  // step over instruction
  block->pc_end += 4;
  block->instructions += 1;
  // cant branch
  return true;
}

static bool op_store(struct riscv_t *rv,
                     uint32_t inst,
                     struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // s-type format
  const int32_t  imm    = dec_stype_imm(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct3 = dec_funct3(inst);

  // arg1 - rv
  cg_mov_r64_r64(cg, cg_rcx, cg_rsi);
  // arg2 - addr
  get_reg(block, rv, cg_edx, rs1);
  cg_add_r32_i32(cg, cg_edx, imm);
  // arg3 - data
  get_reg(block, rv, cg_eax, rs2);
  cg_mov_r64_r64(cg, cg_r8, cg_rax);

  int32_t offset;

  // dispatch by write size
  switch (funct3) {
  case 0: // SB
    // rv->io.mem_write_b(rv, addr, data);
    offset = rv_offset(rv, io.mem_write_b);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  case 1: // SH
    // rv->io.mem_write_s(rv, addr, data);
    offset = rv_offset(rv, io.mem_write_s);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  case 2: // SW
    // rv->io.mem_write_w(rv, addr, data);
    offset = rv_offset(rv, io.mem_write_w);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // step over instruction
  block->pc_end += 4;
  block->instructions += 1;
  // cant branch
  return true;
}

// callback for unhandled op_op instructions
static void handle_op_op(struct riscv_t *rv, uint32_t inst) {
  // r-type decode
  const uint32_t rd     = dec_rd(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct7 = dec_funct7(inst);

  switch (funct7) {
  case 0b0000001:
    // RV32M instructions
    switch (funct3) {
    case 0b010: // MULHSU
      {
        const int64_t a = (int32_t)rv->X[rs1];
        const uint64_t b = rv->X[rs2];
        rv->X[rd] = ((uint64_t)(a * b)) >> 32;
      }
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
        const uint32_t divisor = rv->X[rs2];
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
    }
    break;
  default:
    assert(!"unreachable");
  }
}

static bool op_op(struct riscv_t *rv, uint32_t inst, struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // effective pc
  const uint32_t pc = block->pc_end;
  // r-type decode
  const uint32_t rd     = dec_rd(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct7 = dec_funct7(inst);

  // skip any writes to the zero register
  if (rd == rv_reg_zero) {
    // step over instruction
    block->pc_end += 4;
    block->instructions += 1;
    return true;
  }

  // get operands
  get_reg(block, rv, cg_eax, rs1);
  get_reg(block, rv, cg_ecx, rs2);

  switch (funct7) {
  case 0b0000000:
    switch (funct3) {
    case 0b000: // ADD
      cg_add_r32_r32(cg, cg_eax, cg_ecx);
      break;
    case 0b001: // SLL
      cg_and_r8_i8(cg, cg_cl, 0x1f);
      cg_shl_r32_cl(cg, cg_eax);
      break;
    case 0b010: // SLT
      cg_cmp_r32_r32(cg, cg_eax, cg_ecx);
      cg_setcc_r8(cg, cg_cc_lt, cg_dl);
      cg_movzx_r32_r8(cg, cg_eax, cg_dl);
      break;
    case 0b011: // SLTU
      cg_cmp_r32_r32(cg, cg_eax, cg_ecx);
      cg_setcc_r8(cg, cg_cc_c, cg_dl);
      cg_movzx_r32_r8(cg, cg_eax, cg_dl);
      break;
    case 0b100: // XOR
      cg_xor_r32_r32(cg, cg_eax, cg_ecx);
      break;
    case 0b101: // SRL
      cg_and_r8_i8(cg, cg_cl, 0x1f);
      cg_shr_r32_cl(cg, cg_eax);
      break;
    case 0b110: // OR
      cg_or_r32_r32(cg, cg_eax, cg_ecx);
      break;
    case 0b111: // AND
      cg_and_r32_r32(cg, cg_eax, cg_ecx);
      break;
    default:
      assert(!"unreachable");
      break;
    }
    break;
  case 0b0100000:
    switch (funct3) {
    case 0b000: // SUB
      cg_sub_r32_r32(cg, cg_eax, cg_ecx);
      break;
    case 0b101: // SRA
      cg_and_r8_i8(cg, cg_cl, 0x1f);
      cg_sar_r32_cl(cg, cg_eax);
      break;
    default:
      assert(!"unreachable");
      break;
    }
    break;
#if RISCV_VM_SUPPORT_RV32M
  case 0b0000001:
    // RV32M instructions
    switch (funct3) {
    case 0b000: // MUL
      cg_imul_r32(cg, cg_ecx);
      break;
    case 0b001: // MULH
      cg_imul_r32(cg, cg_ecx);
      cg_mov_r32_r32(cg, cg_eax, cg_edx);
      break;
    case 0b011: // MULHU
      cg_imul_r32(cg, cg_ecx);
      cg_mov_r32_r32(cg, cg_eax, cg_edx);
      break;
    case 0b010: // MULHSU
    case 0b100: // DIV
    case 0b101: // DIVU
    case 0b110: // REM
    case 0b111: // REMU
      // offload to a specific instruction handler
      cg_mov_r64_r64(cg, cg_rcx, cg_rsi);  // arg1 - rv
      cg_mov_r32_i32(cg, cg_edx, inst);    // arg2 - inst
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, jit.handle_op_op));
      // step over instruction
      block->instructions += 1;
      block->pc_end += 4;
      // cant branch
      return true;
    default:
      // cant translate this instruction - terminate block
      cg_mov_r32_i32(cg, cg_eax, pc);
      set_pc(block, rv, cg_eax);
      return false;
    }
    break;
#endif  // RISCV_VM_SUPPORT_RV32M
  default:
    assert(!"unreachable");
    break;
  }

  // rv->X[rd] = rax
  set_reg(block, rv, rd, cg_eax);
  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_lui(struct riscv_t *rv, uint32_t inst, struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t val = dec_utype_imm(inst);
  // rv->X[rd] = val;
  if (rd != rv_reg_zero) {
    cg_mov_r32_i32(cg, cg_eax, val);
    set_reg(block, rv, rd, cg_eax);
  }
  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_branch(struct riscv_t *rv,
                      uint32_t inst,
                      struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // the effective current PC
  const uint32_t pc = block->pc_end;
  // b-type decode
  const uint32_t func3 = dec_funct3(inst);
  const int32_t  imm   = dec_btype_imm(inst);
  const uint32_t rs1   = dec_rs1(inst);
  const uint32_t rs2   = dec_rs2(inst);
  // perform the compare
  get_reg(block, rv, cg_eax, rs1);
  get_reg(block, rv, cg_ecx, rs2);
  cg_cmp_r32_r32(cg, cg_eax, cg_ecx);
  // load both targets
  cg_mov_r32_i32(cg, cg_eax, pc + 4);
  cg_mov_r32_i32(cg, cg_edx, pc + imm);

  // dispatch by branch type
  switch (func3) {
  case 0: // BEQ
    cg_cmov_r32_r32(cg, cg_cc_eq, cg_eax, cg_edx);
    break;
  case 1: // BNE
    cg_cmov_r32_r32(cg, cg_cc_ne, cg_eax, cg_edx);
    break;
  case 4: // BLT
    cg_cmov_r32_r32(cg, cg_cc_lt, cg_eax, cg_edx);
    break;
  case 5: // BGE
    cg_cmov_r32_r32(cg, cg_cc_ge, cg_eax, cg_edx);
    break;
  case 6: // BLTU
    cg_cmov_r32_r32(cg, cg_cc_c, cg_eax, cg_edx);
    break;
  case 7: // BGEU
    cg_cmov_r32_r32(cg, cg_cc_ae, cg_eax, cg_edx);
    break;
  default:
    assert(!"unreachable");
  }
  // load PC with the target
  set_pc(block, rv, cg_eax);
  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // could branch
  return false;
}

static bool op_jalr(struct riscv_t *rv, uint32_t inst, struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // the effective current PC
  const uint32_t pc = block->pc_end;
  // i-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t  imm = dec_itype_imm(inst);

  // jump
  // note: we also clear the least significant bit of pc
  get_reg(block, rv, cg_eax, rs1);
  cg_add_r32_i32(cg, cg_eax, imm);
  cg_and_r32_i32(cg, cg_eax, 0xfffffffe);
  set_pc(block, rv, cg_eax);

  // link
  if (rd != rv_reg_zero) {
    cg_mov_r32_i32(cg, cg_eax, pc + 4);
    set_reg(block, rv, rd, cg_eax);
  }

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // could branch
  return false;
}

static bool op_jal(struct riscv_t *rv, uint32_t inst, struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // the effective current PC
  const uint32_t pc = block->pc_end;
  // j-type decode
  const uint32_t rd  = dec_rd(inst);
  const int32_t rel = dec_jtype_imm(inst);

  // jump
  // note: rel is aligned to a two byte boundary so we dont needs to do any
  //       masking here.
  cg_mov_r32_i32(cg, cg_eax, pc + rel);
  set_pc(block, rv, cg_eax);

  // link
  if (rd != rv_reg_zero) {
    cg_mov_r32_i32(cg, cg_eax, pc + 4);
    set_reg(block, rv, rd, cg_eax);
  }

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // could branch
  return false;
}

static void handle_op_system(struct riscv_t *rv,
                             uint32_t inst) {
  // i-type decode
  const int32_t  imm = dec_itype_imm(inst);
  const int32_t  csr = dec_csr(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rd = dec_rd(inst);

  uint32_t tmp;

  // dispatch by func3 field
  switch (funct3) {
#if RISCV_VM_SUPPORT_Zicsr
  case 1: // CSRRW    (Atomic Read/Write CSR)
    tmp = csr_csrrw(rv, csr, rv->X[rs1]);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 2: // CSRRS    (Atomic Read and Set Bits in CSR)
    tmp = csr_csrrs(rv, csr, (rs1 == rv_reg_zero) ? 0u : rv->X[rs1]);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 3: // CSRRC    (Atomic Read and Clear Bits in CSR)
    tmp = csr_csrrc(rv, csr, (rs1 == rv_reg_zero) ? ~0u : rv->X[rs1]);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 5: // CSRRWI
    tmp = csr_csrrc(rv, csr, rv->X[rs1]);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 6: // CSRRSI
    tmp = csr_csrrs(rv, csr, rs1);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 7: // CSRRCI
    tmp = csr_csrrc(rv, csr, rs1);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
#endif  // RISCV_VM_SUPPORT_Zicsr
  default:
    assert(!"unreachable");
  }
}

static bool op_system(struct riscv_t *rv,
                      uint32_t inst,
                      struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  // the effective current PC
  const uint32_t pc = block->pc_end;
  // i-type decode
  const int32_t  imm    = dec_itype_imm(inst);
  const int32_t  csr    = dec_csr(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rd     = dec_rd(inst);

  // arg1 - rv
  cg_mov_r64_r64(cg, cg_rcx, cg_rsi);

  int32_t offset;

  // set the next PC address
  cg_mov_r32_i32(cg, cg_eax, pc + 4);
  set_pc(block, rv, cg_eax);

  // dispatch by func3 field
  switch (funct3) {
  case 0:
    // dispatch from imm field
    switch (imm) {
    case 0: // ECALL
      offset = rv_offset(rv, io.on_ecall);
      cg_call_r64disp(cg, cg_rsi, offset);
      break;
    case 1: // EBREAK
      offset = rv_offset(rv, io.on_ebreak);
      cg_call_r64disp(cg, cg_rsi, offset);
      break;
    default:
      assert(!"unreachable");
    }
    break;
#if RISCV_VM_SUPPORT_Zicsr
  case 1: // CSRRW    (Atomic Read/Write CSR)
  case 2: // CSRRS    (Atomic Read and Set Bits in CSR)
  case 3: // CSRRC    (Atomic Read and Clear Bits in CSR)
  case 5: // CSRRWI
  case 6: // CSRRSI
  case 7: // CSRRCI
      // offload to a specific instruction handler
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);  // arg1 - rv
    cg_mov_r32_i32(cg, cg_edx, inst);    // arg2 - inst
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, jit.handle_op_system));
    // step over instruction
    block->instructions += 1;
    block->pc_end += 4;
    // cant branch
    return true;
#endif  // RISCV_VM_SUPPORT_Zicsr
  default:
    // cant translate this instruction - terminate block
    cg_mov_r32_i32(cg, cg_eax, pc);
    set_pc(block, rv, cg_eax);
    return false;
  }

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;

  // treat this as a branch point
  return false;
}

#if RISCV_VM_SUPPORT_RV32F
static bool op_load_fp(struct riscv_t *rv,
                       uint32_t inst,
                       struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t  imm = dec_itype_imm(inst);

  // arg1 - rv
  cg_mov_r64_r64(cg, cg_rcx, cg_rsi);
  // arg2 - address
  get_reg(block, rv, cg_edx, rs1);
  cg_add_r32_i32(cg, cg_edx, imm);

  const int32_t offset = rv_offset(rv, io.mem_read_w);
  cg_call_r64disp(cg, cg_rsi, offset);

  set_freg(block, rv, rd, cg_eax);

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;

  // cant branch
  return true;
}

static bool op_store_fp(struct riscv_t *rv,
                        uint32_t inst,
                        struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const int32_t  imm = dec_stype_imm(inst);

  // arg1 - rv
  cg_mov_r64_r64(cg, cg_rcx, cg_rsi);
  // arg2 - address
  get_reg(block, rv, cg_edx, rs1);
  cg_add_r32_i32(cg, cg_edx, imm);
  // arg3 - value
  get_freg(block, rv, cg_eax, rs2);
  cg_mov_r64_r64(cg, cg_r8, cg_rax);

  const int32_t offset = rv_offset(rv, io.mem_write_w);
  cg_call_r64disp(cg, cg_rsi, offset);

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;

  // cant branch
  return true;
}

// callback for unhandled op_fp instructions
static void handle_op_fp(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t rm = dec_funct3(inst); // TODO: rounding!
  const uint32_t funct7 = dec_funct7(inst);
  // dispatch based on func7 (low 2 bits are width)
  switch (funct7) {
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
  case 0b1110000:
    switch (rm) {
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
  default:
    assert(!"unreachable");
  }
}

static bool op_fp(struct riscv_t *rv,
                  uint32_t inst,
                  struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  const uint32_t rd     = dec_rd(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t rm     = dec_funct3(inst); // TODO: rounding!
  const uint32_t funct7 = dec_funct7(inst);

  // dispatch based on func7 (low 2 bits are width)
  switch (funct7) {
  case 0b0000000:  // FADD
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
    cg_addss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs2]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);
    break;
  case 0b0000100:  // FSUB
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
    cg_subss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs2]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);
    break;
  case 0b0001000:  // FMUL
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
    cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs2]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);
    break;
  case 0b0001100:  // FDIV
    cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
    cg_divss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs2]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);
    break;
  case 0b0101100:  // FSQRT
    cg_sqrtss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
    cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);
    break;
  case 0b1100000:
    switch (rs2) {
    // note: these instructions are effectively the same for us currently
    case 0b00000:  // FCVT.W.S
    case 0b00001:  // FCVT.WU.S
      cg_cvttss2si_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, F[rs1]));
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[rd]), cg_eax);
      break;
    default:
      // unsupported instruction
      cg_mov_r32_i32(cg, cg_eax, block->pc_end);
      set_pc(block, rv, cg_eax);
      return false;
    }
    break;
  case 0b1110000:
    switch (rm) {
    case 0b000:  // FMV.X.W
      cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, F[rs1]));
      cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, X[rd]), cg_eax);
      break;
    case 0b001:  // FCLASS.S
      cg_mov_r64_r64(cg, cg_rcx, cg_rsi);   // arg1 - rv
      cg_mov_r32_i32(cg, cg_edx, inst);     // arg2 - inst
      cg_call_r64disp(cg, cg_rsi, rv_offset(rv, jit.handle_op_fp));
      // step over instruction
      block->instructions += 1;
      block->pc_end += 4;
      // cant branch
      return true;
    default:
      cg_mov_r32_i32(cg, cg_eax, block->pc_end);
      set_pc(block, rv, cg_eax);
      return false;
    }
    break;
  case 0b1101000:
    switch (rs2) {
      // note: these instructions are affectively the same for us currently
    case 0b00000:  // FCVT.S.W
    case 0b00001:  // FCVT.S.WU
      cg_cvtsi2ss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, X[rs1]));
      cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);
      break;
    default:
      cg_mov_r32_i32(cg, cg_eax, block->pc_end);
      set_pc(block, rv, cg_eax);
      return false;
    }
    break;
  case 0b1111000:  // FMV.W.X
    cg_mov_r32_r64disp(cg, cg_eax, cg_rsi, rv_offset(rv, X[rs1]));
    cg_mov_r64disp_r32(cg, cg_rsi, rv_offset(rv, F[rd]), cg_eax);
    break;
  case 0b0010000: // FSGNJ.S, FSGNJN.S, FSGNJX.S
  case 0b0010100: // FMIN, FMAX
  case 0b1010000: // FEQ.S, FLT.S, FLE.S
    cg_mov_r64_r64(cg, cg_rcx, cg_rsi);   // arg1 - rv
    cg_mov_r32_i32(cg, cg_edx, inst);     // arg2 - inst
    cg_call_r64disp(cg, cg_rsi, rv_offset(rv, jit.handle_op_fp));
    // step over instruction
    block->instructions += 1;
    block->pc_end += 4;
    // cant branch
    return true;
  default:
    // unsupported instruction
    cg_mov_r32_i32(cg, cg_eax, block->pc_end);
    set_pc(block, rv, cg_eax);
    return false;
  }

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_madd(struct riscv_t *rv,
                    uint32_t inst,
                    struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  const uint32_t rd = dec_rd(inst);
  const uint32_t rm = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);

  // rv->F[rd] = rv->F[rs1] * rv->F[rs2] + rv->F[rs3];
  cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
  cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs2]));
  cg_addss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs3]));
  cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_msub(struct riscv_t *rv,
                    uint32_t inst,
                    struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  const uint32_t rd = dec_rd(inst);
  const uint32_t rm = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);

  // rv->F[rd] = rv->F[rs1] * rv->F[rs2] - rv->F[rs3];
  cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
  cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs2]));
  cg_subss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs3]));
  cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_nmadd(struct riscv_t *rv,
                     uint32_t inst,
                     struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  const uint32_t rd = dec_rd(inst);
  const uint32_t rm = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);

  // compute
  // rv->F[rd] = -(rv->F[rs1] * rv->F[rs2]) - rv->F[rs3];

  // multiply
  cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
  cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs2]));
  // negate
  cg_mov_r32_xmm(cg, cg_eax, cg_xmm0);
  cg_xor_r32_i32(cg, cg_eax, 0x80000000);
  cg_mov_xmm_r32(cg, cg_xmm0, cg_eax);
  // subtract
  cg_subss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs3]));
  // store
  cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // cant branch
  return true;
}

static bool op_nmsub(struct riscv_t *rv,
                    uint32_t inst,
                    struct block_t *block) {

  struct cg_state_t *cg = &block->cg;

  const uint32_t rd = dec_rd(inst);
  const uint32_t rm = dec_funct3(inst);      // todo
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t fmt = dec_r4type_fmt(inst);  // unused
  const uint32_t rs3 = dec_r4type_rs3(inst);

  // compute
  // rv->F[rd] = rv->F[rs3] - (rv->F[rs1] * rv->F[rs2]);

  // multiply
  cg_movss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs1]));
  cg_mulss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs2]));
  // negate
  cg_mov_r32_xmm(cg, cg_eax, cg_xmm0);
  cg_xor_r32_i32(cg, cg_eax, 0x80000000);
  cg_mov_xmm_r32(cg, cg_xmm0, cg_eax);
  // add
  cg_addss_xmm_r64disp(cg, cg_xmm0, cg_rsi, rv_offset(rv, F[rs3]));
  // store
  cg_movss_r64disp_xmm(cg, cg_rsi, rv_offset(rv, F[rd]), cg_xmm0);

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // cant branch
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
typedef bool(*opcode_t)(struct riscv_t *rv,
                        uint32_t inst,
                        struct block_t *block);

// opcode dispatch table
static const opcode_t opcodes[] = {
//  000        001          010       011          100        101       110   111
    op_load,   op_load_fp,  NULL,     NULL,        op_op_imm, op_auipc, NULL, NULL, // 00
    op_store,  op_store_fp, NULL,     NULL,        op_op,     op_lui,   NULL, NULL, // 01
    op_madd,   op_msub,     op_nmsub, op_nmadd,    op_fp,     NULL,     NULL, NULL, // 10
    op_branch, op_jalr,     NULL,     op_jal,      op_system, NULL,     NULL, NULL, // 11
};

static void rv_translate_block(struct riscv_t *rv, struct block_t *block) {
  assert(rv && block);

  struct cg_state_t *cg = &block->cg;

  // setup the basic block
  block->instructions = 0;
  block->pc_start = rv->PC;
  block->pc_end = rv->PC;

  // prologue
  gen_prologue(block, rv);

  // translate the basic block
  for (;;) {
    // fetch the next instruction
    const uint32_t inst = rv->io.mem_ifetch(rv, block->pc_end);
    const uint32_t index = (inst & INST_6_2) >> 2;
    // find translation function
    const opcode_t op = opcodes[index];
    if (!op) {
      // we dont have a handler for this instruction so end basic block
      // make sure PC gets updated
      cg_mov_r32_i32(cg, cg_eax, block->pc_end);
      set_pc(block, rv, cg_eax);
      break;
    }
    if (!op(rv, inst, block)) {
      break;
    }
  }

  // epilogue
  gen_epilogue(block, rv);
  cg_ret(cg);
}

struct block_t *block_find_or_translate(struct riscv_t *rv,
                                        struct block_t *prev) {
  // lookup the next block in the block map
  struct block_t *next = block_find(&rv->jit, rv->PC);
  // translate if we didnt find one
  if (!next) {
    next = block_alloc(&rv->jit);
    assert(next);
    rv_translate_block(rv, next);
    block_finish(&rv->jit, next);
    // update the block predictor
    // note: if the block predictor gives us a win when we
    //       translate a new block but gives us a huge penalty when
    //       updated after we find a new block.  didnt expect that.
    if (prev) {
      prev->predict = next;
    }
  }
  assert(next);
  return next;
}

void rv_step(struct riscv_t *rv, int32_t cycles) {

  // find or translate a block for our starting PC
  struct block_t *block = block_find_or_translate(rv, NULL);
  assert(block);

  const uint64_t cycles_start = rv->csr_cycle;
  const uint64_t cycles_target = rv->csr_cycle + cycles;

  // loop until we hit out cycle target
  while (rv->csr_cycle < cycles_target && !rv->halt) {

    // try to predict the next block
    // note: block predition gives us ~100 MIPS boost.
    if (block->predict && block->predict->pc_start == rv->PC) {
      block = block->predict;
    }
    else {
      // lookup the next block in the block map or translate a new block
      struct block_t *next = block_find_or_translate(rv, block);
      // move onto the next block
      block = next;
    }

    // we should have a block by now
    assert(block);

    // call the translated block
    typedef void(*call_block_t)(struct riscv_t *);
    call_block_t c = (call_block_t)block->code;
    // printf("// %08x\n", rv->PC);
    c(rv);

    // increment the cycles csr
    rv->csr_cycle += block->instructions;

    // if this block has no instructions we cant make forward progress so
    // must fallback to instruction emulation
    if (!block->instructions) {
      assert(!"unable to execute empty block");
    }
  }
}

bool rv_init_jit(struct riscv_t *rv) {

  struct riscv_jit_t *jit = &rv->jit;

  // allocate the block map which maps address to blocks
  if (jit->block_map == NULL) {
    jit->block_map_size = map_size;
    jit->block_map = malloc(map_size * sizeof(struct block_t*));
    memset(jit->block_map, 0, map_size * sizeof(struct block_t*));
  }

  // allocate block/code storage space
  if (jit->start == NULL) {
    void *ptr = sys_alloc_exec_mem(code_size);
    memset(ptr, 0xcc, code_size);
    jit->start = ptr;
    jit->head = ptr;
    jit->end = jit->start + code_size;
  }

  // setup nonjit instruction callbacks
  jit->handle_op_op     = handle_op_op;
  jit->handle_op_fp     = handle_op_fp;
  jit->handle_op_system = handle_op_system;

  return true;
}
