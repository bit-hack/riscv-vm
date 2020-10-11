#include <assert.h>
#include <string.h>

#include "../riscv_emu/riscv_private.h"

#include "ir.h"


enum {
  op_imm,

  op_ld_reg,
  op_st_reg,

  op_st_pc,
  op_branch,

  op_add,
  op_sub,
  op_and,
  op_or,
  op_xor,
  op_shr,
  op_sar,
  op_shl,
  op_mul,
  op_imul,

  op_eq,
  op_neq,
  op_lt,
  op_ge,
  op_ltu,
  op_geu,

  op_sb,
  op_sh,
  op_sw,
  op_lb,
  op_lh,
  op_lw,
  op_lbu,
  op_lhu,
  op_ecall,
  op_ebreak
};

static struct ir_inst_t *ir_alloc(struct ir_block_t *block) {
  assert(block);
  struct ir_inst_t *i = block->inst + (block->head++);
  assert(block->head < IR_MAX_INST);
  memset(i, 0, sizeof(struct ir_inst_t));
  return i;
}

void ir_init(struct ir_block_t *block) {
  assert(block);
  memset(block, 0, sizeof(struct ir_block_t));
}

struct ir_inst_t *ir_imm(struct ir_block_t *block, int32_t imm) {
  assert(block);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_imm;
  i->imm = imm;
  return i;
}

struct ir_inst_t *ir_ld_reg(struct ir_block_t *block, int32_t offset) {
  assert(block);
  struct ir_inst_t *i = block->inst + (block->head++);
  i->op = op_ld_reg;
  i->offset = offset;
  return i;
}

struct ir_inst_t *ir_st_reg(struct ir_block_t *block, int32_t offset, struct ir_inst_t *val) {
  assert(block && val);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_st_reg;
  i->offset = offset;
  i->value = val;
  val->parent = i;
  return i;
}

struct ir_inst_t *ir_st_pc(struct ir_block_t *block, struct ir_inst_t *val) {
  assert(block && val);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_st_pc;
  i->value = val;
  val->parent = i;
  return i;
}

struct ir_inst_t *ir_add(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_add;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_sub(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_sub;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_and(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_and;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_or(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_or;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_xor(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_xor;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_shr(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_shr;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_sar(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_sar;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_shl(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_shl;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_mul(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_mul;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_imul(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_imul;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_sb(struct ir_block_t *block, struct ir_inst_t *addr, struct ir_inst_t *val) {
  assert(block && addr && val);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_sb;
  i->value = val;
  i->addr = addr;
  addr->parent = i;
  val->parent = i;
  return i;
}

struct ir_inst_t *ir_sh(struct ir_block_t *block, struct ir_inst_t *addr, struct ir_inst_t *val) {
  assert(block && addr && val);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_sh;
  i->value = val;
  i->addr = addr;
  addr->parent = i;
  val->parent = i;
  return i;
}

struct ir_inst_t *ir_sw(struct ir_block_t *block, struct ir_inst_t *addr, struct ir_inst_t *val) {
  assert(block && addr && val);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_sw;
  i->value = val;
  i->addr = addr;
  addr->parent = i;
  val->parent = i;
  return i;
}

struct ir_inst_t *ir_lb(struct ir_block_t *block, struct ir_inst_t *addr) {
  assert(block && addr);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_lb;
  i->addr = addr;
  addr->parent = i;
  return i;
}

struct ir_inst_t *ir_lh(struct ir_block_t *block, struct ir_inst_t *addr) {
  assert(block && addr);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_lh;
  i->addr = addr;
  addr->parent = i;
  return i;
}

struct ir_inst_t *ir_lw(struct ir_block_t *block, struct ir_inst_t *addr) {
  assert(block && addr);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_lw;
  i->addr = addr;
  addr->parent = i;
  return i;
}

struct ir_inst_t *ir_lbu(struct ir_block_t *block, struct ir_inst_t *addr) {
  assert(block && addr);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_lbu;
  i->addr = addr;
  addr->parent = i;
  return i;
}

struct ir_inst_t *ir_lhu(struct ir_block_t *block, struct ir_inst_t *addr) {
  assert(block && addr);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_lhu;
  i->addr = addr;
  addr->parent = i;
  return i;
}

struct ir_inst_t *ir_ecall(struct ir_block_t *block) {
  assert(block);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_ecall;
  return i;
}

struct ir_inst_t *ir_ebreak(struct ir_block_t *block) {
  assert(block);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_ebreak;
  return i;
}

struct ir_inst_t *ir_eq(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_eq;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_neq(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_neq;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_lt(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_lt;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_ge(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_ge;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_ltu(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_ltu;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_geu(struct ir_block_t *block, struct ir_inst_t *lhs, struct ir_inst_t *rhs) {
  assert(block && lhs && rhs);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_geu;
  i->lhs = lhs;
  i->rhs = rhs;
  lhs->parent = i;
  rhs->parent = i;
  return i;
}

struct ir_inst_t *ir_branch(struct ir_block_t *block,
                            struct ir_inst_t *cond,
                            struct ir_inst_t *taken,
                            struct ir_inst_t *not_taken) {
  assert(block && cond && taken && not_taken);
  struct ir_inst_t *i = ir_alloc(block);
  i->op = op_branch;
  i->cond = cond;
  i->lhs = taken;
  i->rhs = not_taken;
  cond->parent = i;
  taken->parent = i;
  not_taken->parent = i;
  return i;
}

static int32_t eval(struct riscv_t *rv, const struct ir_inst_t *i) {
  switch (i->op) {
  case op_imm:
    return i->imm;
  case op_ld_reg:
    return (int32_t)(rv->X[i->offset]);
  case op_st_reg:
    rv->X[i->offset] = eval(rv, i->value);
    break;
  case op_st_pc:
    rv->PC = eval(rv, i->value);
    break;
  case op_branch:
    rv->PC = eval(rv, i->cond) ? eval(rv, i->lhs) : eval(rv, i->rhs);
    break;
  case op_add:
    return eval(rv, i->lhs) + eval(rv, i->rhs);
  case op_sub:
    return eval(rv, i->lhs) - eval(rv, i->rhs);
  case op_and:
    return eval(rv, i->lhs) & eval(rv, i->rhs);
  case op_or:
    return eval(rv, i->lhs) | eval(rv, i->rhs);
  case op_xor:
    return eval(rv, i->lhs) ^ eval(rv, i->rhs);
  case op_shr:
    return (uint32_t)eval(rv, i->lhs) >> (uint32_t)eval(rv, i->rhs);
  case op_sar:
    return eval(rv, i->lhs) >> eval(rv, i->rhs);
  case op_shl:
    return (uint32_t)eval(rv, i->lhs) << (uint32_t)eval(rv, i->rhs);
  case op_mul:
    return (uint32_t)eval(rv, i->lhs) * (uint32_t)eval(rv, i->rhs);
  case op_imul:
    return eval(rv, i->lhs) * eval(rv, i->rhs);
  case op_eq:
    return eval(rv, i->lhs) == eval(rv, i->rhs);
  case op_neq:
    return eval(rv, i->lhs) != eval(rv, i->rhs);
  case op_lt:
    return eval(rv, i->lhs) < eval(rv, i->rhs);
  case op_ge:
    return eval(rv, i->lhs) >= eval(rv, i->rhs);
  case op_ltu:
    return (uint32_t)eval(rv, i->lhs) < (uint32_t)eval(rv, i->rhs);
  case op_geu:
    return (uint32_t)eval(rv, i->lhs) >= (uint32_t)eval(rv, i->rhs);
  case op_sb:
    rv->io.mem_write_b(rv, (uint32_t)eval(rv, i->addr), eval(rv, i->value));
    break;
  case op_sh:
    rv->io.mem_write_s(rv, (uint32_t)eval(rv, i->addr), eval(rv, i->value));
    break;
  case op_sw:
    rv->io.mem_write_w(rv, (uint32_t)eval(rv, i->addr), eval(rv, i->value));
    break;
  case op_lb:
    return (int8_t)rv->io.mem_read_b(rv, (uint32_t)eval(rv, i->addr));
  case op_lh:
    return (int16_t)rv->io.mem_read_s(rv, (uint32_t)eval(rv, i->addr));
  case op_lw:
    return (int32_t)rv->io.mem_read_w(rv, (uint32_t)eval(rv, i->addr));
  case op_lbu:
    return (uint8_t)rv->io.mem_read_b(rv, (uint32_t)eval(rv, i->addr));
  case op_lhu:
    return (uint16_t)rv->io.mem_read_s(rv, (uint32_t)eval(rv, i->addr));
  case op_ecall:
    rv->io.on_ecall(rv, 0, 0);
    break;
  case op_ebreak:
    rv->io.on_ebreak(rv, 0, 0);
    break;
  default:
    assert(!"unreachable");
  }
  // this should be a root instruction
  assert(i->parent == NULL);
  return 0;
}

void ir_eval(struct ir_block_t *block, struct riscv_t *rv) {
  for (int i = 0; i < block->head; ++i) {
    const struct ir_inst_t *inst = block->inst + i;
    if (inst->parent != NULL) {
      continue;
    }
    eval(rv, inst);
  }
}
