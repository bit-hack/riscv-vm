#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
static const uint32_t code_size = 1024 * 1024 * 4;

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

static void set_reg(struct block_t *block, struct riscv_t *rv, uint32_t dst, cg_r32_t src) {

  struct cg_state_t *cg = &block->cg;

  if (dst != rv_reg_zero) {
    const int32_t offset = rv_offset(rv, X[dst]);
    cg_mov_r64disp_r32(cg, cg_rsi, offset, src);
  }
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

static bool op_load(struct riscv_t *rv, uint32_t inst, struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct cg_state_t *cg = &block->cg;
  struct ir_block_t *z = &builder->ir;

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

  // generate load address
  struct ir_inst_t *ir_addr = ir_add(z, ir_ld_reg(z, rs1), ir_imm(z, imm));
  struct ir_inst_t *ir_load = NULL;

  // dispatch by read size
  switch (funct3) {
  case 0: // LB
    ir_load = ir_lb(z, ir_addr);

    offset = rv_offset(rv, io.mem_read_b);
    cg_call_r64disp(cg, cg_rsi, offset);
    cg_movsx_r32_r8(cg, cg_eax, cg_al);
    break;
  case 1: // LH
    ir_load = ir_lh(z, ir_addr);

    offset = rv_offset(rv, io.mem_read_s);
    cg_call_r64disp(cg, cg_rsi, offset);
    cg_movsx_r32_r16(cg, cg_eax, cg_ax);
    break;
  case 2: // LW
    ir_load = ir_lw(z, ir_addr);

    offset = rv_offset(rv, io.mem_read_w);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  case 4: // LBU
    ir_load = ir_lbu(z, ir_addr);

    offset = rv_offset(rv, io.mem_read_b);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  case 5: // LHU
    ir_load = ir_lhu(z, ir_addr);

    offset = rv_offset(rv, io.mem_read_s);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = rax
  assert(ir_load);
  ir_st_reg(z, rd, ir_load);
  set_reg(block, rv, rd, cg_eax);
  // step over instruction
  block->pc_end += 4;
  block->instructions += 1;
  // cant branch
  return true;
}

static bool op_op_imm(struct riscv_t *rv,
                      uint32_t inst,
                      struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct cg_state_t *cg = &block->cg;
  struct ir_block_t *z = &builder->ir;

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

  struct ir_inst_t *lhs = ir_ld_reg(z, rs1);
  struct ir_inst_t *res = NULL;

  // dispatch operation type
  switch (funct3) {
  case 0: // ADDI
    res = ir_add(z, lhs, ir_imm(z, imm));
    cg_add_r32_i32(cg, cg_eax, imm);
    break;
  case 1: // SLLI
    res = ir_shl(z, lhs, ir_imm(z, imm & 0x1f));
    cg_shl_r32_i8(cg, cg_eax, imm & 0x1f);
    break;
  case 2: // SLTI
    res = ir_lt(z, lhs, ir_imm(z, imm));
    cg_cmp_r32_i32(cg, cg_eax, imm);
    cg_setcc_r8(cg, cg_cc_lt, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    break;
  case 3: // SLTIU
    res = ir_ltu(z, lhs, ir_imm(z, imm));
    cg_cmp_r32_i32(cg, cg_eax, imm);
    cg_setcc_r8(cg, cg_cc_c, cg_dl);
    cg_movzx_r32_r8(cg, cg_eax, cg_dl);
    break;
  case 4: // XORI
    res = ir_xor(z, lhs, ir_imm(z, imm));
    cg_xor_r32_i32(cg, cg_eax, imm);
    break;
  case 5:
    if (imm & ~0x1f) {
      // SRAI
      res = ir_sar(z, lhs, ir_imm(z, imm & 0x1f));
      cg_sar_r32_i8(cg, cg_eax, imm & 0x1f);
    }
    else {
      // SRLI
      res = ir_shr(z, lhs, ir_imm(z, imm & 0x1f));
      cg_shr_r32_i8(cg, cg_eax, imm & 0x1f);
    }
    break;
  case 6: // ORI
    res = ir_or(z, lhs, ir_imm(z, imm));
    cg_or_r32_i32(cg, cg_eax, imm);
    break;
  case 7: // ANDI
    res = ir_and(z, lhs, ir_imm(z, imm));
    cg_and_r32_i32(cg, cg_eax, imm);
    break;
  default:
    assert(!"unreachable");
    break;
  }
  // rv->X[rd] = eax
  assert(res);
  ir_st_reg(z, rd, res);
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
                     struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct ir_block_t *z = &builder->ir;
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
  ir_st_reg(z, rd, ir_imm(z, pc + imm));
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
                     struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct ir_block_t *z = &builder->ir;
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

  // generate store address
  struct ir_inst_t *ir_addr = ir_add(z, ir_ld_reg(z, rs1), ir_imm(z, imm));

  // dispatch by write size
  switch (funct3) {
  case 0: // SB
    ir_sb(z, ir_addr, ir_ld_reg(z, rs2));
    // rv->io.mem_write_b(rv, addr, data);
    offset = rv_offset(rv, io.mem_write_b);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  case 1: // SH
    ir_sh(z, ir_addr, ir_ld_reg(z, rs2));
    // rv->io.mem_write_s(rv, addr, data);
    offset = rv_offset(rv, io.mem_write_s);
    cg_call_r64disp(cg, cg_rsi, offset);
    break;
  case 2: // SW
    ir_sw(z, ir_addr, ir_ld_reg(z, rs2));
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

static bool op_op(struct riscv_t *rv,
                  uint32_t inst,
                  struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct ir_block_t *z = &builder->ir;
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

  struct ir_inst_t *lhs = ir_ld_reg(z, rs1);
  struct ir_inst_t *rhs = ir_ld_reg(z, rs2);
  struct ir_inst_t *res = NULL;

  switch (funct7) {
  case 0b0000000:
    switch (funct3) {
    case 0b000: // ADD
      res = ir_add(z, lhs, rhs);
      cg_add_r32_r32(cg, cg_eax, cg_ecx);
      break;
    case 0b001: // SLL
      res = ir_add(z, lhs, ir_and(z, rhs, ir_imm(z, 0x1f)));
      cg_and_r8_i8(cg, cg_cl, 0x1f);
      cg_shl_r32_cl(cg, cg_eax);
      break;
    case 0b010: // SLT
      res = ir_lt(z, lhs, rhs);
      cg_cmp_r32_r32(cg, cg_eax, cg_ecx);
      cg_setcc_r8(cg, cg_cc_lt, cg_dl);
      cg_movzx_r32_r8(cg, cg_eax, cg_dl);
      break;
    case 0b011: // SLTU
      res = ir_ltu(z, lhs, rhs);
      cg_cmp_r32_r32(cg, cg_eax, cg_ecx);
      cg_setcc_r8(cg, cg_cc_c, cg_dl);
      cg_movzx_r32_r8(cg, cg_eax, cg_dl);
      break;
    case 0b100: // XOR
      res = ir_xor(z, lhs, rhs);
      cg_xor_r32_r32(cg, cg_eax, cg_ecx);
      break;
    case 0b101: // SRL
      res = ir_shl(z, lhs, ir_and(z, rhs, ir_imm(z, 0x1f)));
      cg_and_r8_i8(cg, cg_cl, 0x1f);
      cg_shr_r32_cl(cg, cg_eax);
      break;
    case 0b110: // OR
      res = ir_or(z, lhs, rhs);
      cg_or_r32_r32(cg, cg_eax, cg_ecx);
      break;
    case 0b111: // AND
      res = ir_and(z, lhs, rhs);
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
      res = ir_sub(z, lhs, rhs);
      cg_sub_r32_r32(cg, cg_eax, cg_ecx);
      break;
    case 0b101: // SRA
      res = ir_sar(z, lhs, ir_and(z, rhs, ir_imm(z, 0x1f)));
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
    case 0b010: // MULHSU
      // const int64_t a = (int32_t)rv->X[rs1];
      // const uint64_t b = rv->X[rs2];
      // rv->X[rd] = ((uint64_t)(a * b)) >> 32;

      // cant translate this instruction - terminate block
      cg_mov_r32_i32(cg, cg_eax, pc);
      set_pc(block, rv, cg_eax);
      return false;

      break;
    case 0b011: // MULHU
      cg_imul_r32(cg, cg_ecx);
      cg_mov_r32_r32(cg, cg_eax, cg_edx);
      break;
    case 0b100: // DIV
    {
      // const int32_t dividend = (int32_t)rv->X[rs1];
      // const int32_t divisor = (int32_t)rv->X[rs2];
      // if (divisor == 0) {
      //   rv->X[rd] = ~0u;
      // }
      // else if (divisor == -1 && rv->X[rs1] == 0x80000000u) {
      //   rv->X[rd] = rv->X[rs1];
      // }
      // else {
      //   rv->X[rd] = dividend / divisor;
      // }

      // cant translate this instruction - terminate block
      cg_mov_r32_i32(cg, cg_eax, pc);
      set_pc(block, rv, cg_eax);
      return false;

    }
    break;
    case 0b101: // DIVU
    {
      // const uint32_t dividend = rv->X[rs1];
      // const uint32_t divisor = rv->X[rs2];
      // if (divisor == 0) {
      //   rv->X[rd] = ~0u;
      // }
      // else {
      //   rv->X[rd] = dividend / divisor;
      // }

      // cant translate this instruction - terminate block
      cg_mov_r32_i32(cg, cg_eax, pc);
      set_pc(block, rv, cg_eax);
      return false;

    }
    break;
    case 0b110: // REM
    {
      // const int32_t dividend = rv->X[rs1];
      // const int32_t divisor = rv->X[rs2];
      // if (divisor == 0) {
      //   rv->X[rd] = dividend;
      // }
      // else if (divisor == -1 && rv->X[rs1] == 0x80000000u) {
      //   rv->X[rd] = 0;
      // }
      // else {
      //   rv->X[rd] = dividend % divisor;
      // }

      // cant translate this instruction - terminate block
      cg_mov_r32_i32(cg, cg_eax, pc);
      set_pc(block, rv, cg_eax);
      return false;

    }
    break;
    case 0b111: // REMU
    {
      // const uint32_t dividend = rv->X[rs1];
      // const uint32_t divisor = rv->X[rs2];
      // if (divisor == 0) {
      //   rv->X[rd] = dividend;
      // }
      // else {
      //   rv->X[rd] = dividend % divisor;
      // }

      // cant translate this instruction - terminate block
      cg_mov_r32_i32(cg, cg_eax, pc);
      set_pc(block, rv, cg_eax);
      return false;

    }
    break;
    default:
      assert(!"unreachable");
      break;
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

static bool op_lui(struct riscv_t *rv,
                   uint32_t inst,
                   struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct ir_block_t *z = &builder->ir;
  struct cg_state_t *cg = &block->cg;

  // u-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t val = dec_utype_imm(inst);
  // rv->X[rd] = val;
  if (rd != rv_reg_zero) {
    ir_st_reg(z, rd, ir_imm(z, val));
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
                      struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct ir_block_t *z = &builder->ir;
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

  struct ir_inst_t *lhs = ir_ld_reg(z, rs1);
  struct ir_inst_t *rhs = ir_ld_reg(z, rs2);

  struct ir_inst_t *next = ir_imm(z, pc + 4);
  struct ir_inst_t *targ = ir_imm(z, pc + imm);
  struct ir_inst_t *cond = NULL;

  // dispatch by branch type
  switch (func3) {
  case 0: // BEQ
    cond = ir_eq(z, lhs, rhs);
    cg_cmov_r32_r32(cg, cg_cc_eq, cg_eax, cg_edx);
    break;
  case 1: // BNE
    cond = ir_neq(z, lhs, rhs);
    cg_cmov_r32_r32(cg, cg_cc_ne, cg_eax, cg_edx);
    break;
  case 4: // BLT
    cond = ir_lt(z, lhs, rhs);
    cg_cmov_r32_r32(cg, cg_cc_lt, cg_eax, cg_edx);
    break;
  case 5: // BGE
    cond = ir_ge(z, lhs, rhs);
    cg_cmov_r32_r32(cg, cg_cc_ge, cg_eax, cg_edx);
    break;
  case 6: // BLTU
    cond = ir_ltu(z, lhs, rhs);
    cg_cmov_r32_r32(cg, cg_cc_c, cg_eax, cg_edx);
    break;
  case 7: // BGEU
    cond = ir_geu(z, lhs, rhs);
    cg_cmov_r32_r32(cg, cg_cc_ae, cg_eax, cg_edx);
    break;
  default:
    assert(!"unreachable");
  }
  // load PC with the target
  ir_branch(z, cond, targ, next);
  set_pc(block, rv, cg_eax);
  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // could branch
  return false;
}

static bool op_jalr(struct riscv_t *rv,
                    uint32_t inst,
                    struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct ir_block_t *z = &builder->ir;
  struct cg_state_t *cg = &block->cg;

  // the effective current PC
  const uint32_t pc = block->pc_end;
  // i-type decode
  const uint32_t rd  = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const int32_t  imm = dec_itype_imm(inst);

  // jump
  // note: we also clear the least significant bit of pc

  struct ir_inst_t *addr =
    ir_and(z, ir_add(z, ir_ld_reg(z, rs1), ir_imm(z, imm)), ir_imm(z, 0xfffffffe));
  ir_st_pc(z, addr);

  get_reg(block, rv, cg_eax, rs1);
  cg_add_r32_i32(cg, cg_eax, imm);
  cg_and_r32_i32(cg, cg_eax, 0xfffffffe);
  set_pc(block, rv, cg_eax);

  // link
  if (rd != rv_reg_zero) {
    ir_st_reg(z, rd, ir_imm(z, pc + 4));
    cg_mov_r32_i32(cg, cg_eax, pc + 4);
    set_reg(block, rv, rd, cg_eax);
  }

#if RISCV_SUPPORT_MACHINE
  // check for exception
  // if (rv->PC & 0x3) {
  //   raise_exception(rv, rv_except_inst_misaligned);
  // }
#endif

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // could branch
  return false;
}

static bool op_jal(struct riscv_t *rv,
                   uint32_t inst,
                   struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct ir_block_t *z = &builder->ir;
  struct cg_state_t *cg = &block->cg;

  // the effective current PC
  const uint32_t pc = block->pc_end;
  // j-type decode
  const uint32_t rd  = dec_rd(inst);
  const int32_t rel = dec_jtype_imm(inst);

  // jump
  // note: rel is aligned to a two byte boundary so we dont needs to do any
  //       masking here.

  ir_st_pc(z, ir_imm(z, pc + rel));
  cg_mov_r32_i32(cg, cg_eax, pc + rel);
  set_pc(block, rv, cg_eax);

  // link
  if (rd != rv_reg_zero) {
    ir_st_reg(z, rd, ir_imm(z, pc + 4));
    cg_mov_r32_i32(cg, cg_eax, pc + 4);
    set_reg(block, rv, rd, cg_eax);
  }

#if RISCV_SUPPORT_MACHINE
  // check alignment of PC
  // if (rv->PC & 0x3) {
  //   raise_exception(rv, rv_except_inst_misaligned);
  // }
#endif

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;
  // could branch
  return false;
}

static bool op_system(struct riscv_t *rv,
                      uint32_t inst,
                      struct block_builder_t *builder) {
  struct block_t *block = builder->block;
  struct ir_block_t *z = &builder->ir;
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
  // arg2 - pc
  cg_mov_r32_i32(cg, cg_edx, pc);
  // arg3 - instruction
  cg_mov_r64_i32(cg, cg_r8, inst);

  int32_t offset;

  // dispatch by func3 field
  switch (funct3) {
  case 0:
    // dispatch from imm field
    switch (imm) {
    case 0: // ECALL
      ir_ecall(z);
      offset = rv_offset(rv, io.on_ecall);
      cg_call_r64disp(cg, cg_rsi, offset);
      break;
    case 1: // EBREAK
      ir_ebreak(z);
      offset = rv_offset(rv, io.on_ebreak);
      cg_call_r64disp(cg, cg_rsi, offset);
      break;
    default:
      assert(!"unreachable");
    }
    break;
  case 1:
  case 2:
  case 3:
    // TODO: CSRRW, CSRRS, CSRRC
    break;
  default:
    // cant translate this instruction - terminate block
    cg_mov_r32_i32(cg, cg_eax, pc);
    set_pc(block, rv, cg_eax);
    return false;
  }

  // step over instruction
  block->instructions += 1;
  block->pc_end += 4;

  // XXX: assume we wont branch for now but will need to be updated later
  return true;
}

// opcode handler type
typedef bool(*opcode_t)(struct riscv_t *rv,
                        uint32_t inst,
                        struct block_builder_t *builder);

// opcode dispatch table
static const opcode_t opcodes[] = {
//  000        001          010       011          100        101       110   111
    op_load,   NULL,        NULL,     NULL,        op_op_imm, op_auipc, NULL, NULL, // 00
    op_store,  NULL,        NULL,     NULL,        op_op,     op_lui,   NULL, NULL, // 01
    NULL,      NULL,        NULL,     NULL,        NULL,      NULL,     NULL, NULL, // 10
    op_branch, op_jalr,     NULL,     op_jal,      op_system, NULL,     NULL, NULL, // 11
};

static void rv_translate_block(struct riscv_t *rv, struct block_t *block) {
  assert(rv && block);

  struct block_builder_t builder;
  builder.block = block;
  ir_init(&builder.ir);

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
    if (!op(rv, inst, &builder)) {
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

bool rv_step_jit(struct riscv_t *rv, const uint64_t cycles_target) {

  // find or translate a block for our starting PC
  struct block_t *block = block_find_or_translate(rv, NULL);
  assert(block);

  // loop until we hit out cycle target
  while (rv->csr_cycle < cycles_target) {

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
    c(rv);

    // increment the cycles csr
    rv->csr_cycle += block->instructions;

    // if this block has no instructions we cant make forward progress so
    // must fallback to instruction emulation
    if (!block->instructions) {
      return false;
    }
  }

  // hit our cycle target
  return true;
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

  return true;
}
