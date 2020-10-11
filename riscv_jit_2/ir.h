#pragma once
#include <stdint.h>


enum {
  inst_imm,

  inst_ld_reg,
  inst_st_reg,

  inst_st_pc,
  inst_branch,

  inst_add,
  inst_sub,
  inst_and,
  inst_or,
  inst_xor,
  inst_shr,
  inst_sar,
  inst_shl,
  inst_mul,
  inst_mulh,
  inst_mulhsu,
  inst_mulhu,
  inst_div,
  inst_divu,
  inst_rem,
  inst_remu,

  inst_eq,
  inst_neq,
  inst_lt,
  inst_ge,
  inst_ltu,
  inst_geu,

  inst_sb,
  inst_sh,
  inst_sw,
  inst_lb,
  inst_lh,
  inst_lw,
  inst_lbu,
  inst_lhu,
  inst_ecall,
  inst_ebreak
};

struct ir_inst_t {
  int32_t op;
  // immediate / offset / condition
  union {
    int32_t imm;
    int32_t offset;
    struct ir_inst_t *cond;
  };
  // lhs / value / taken
  union {
    struct ir_inst_t *lhs;
    struct ir_inst_t *value;
  };
  // rhs / addr / not taken
  union {
    struct ir_inst_t *rhs;
    struct ir_inst_t *addr;
  };
  // parent node
  struct ir_inst_t *parent;
};

struct ir_block_t {
  int32_t head;
  const void *end;
  // anything with a NULL parent is a root node to be emitted
  struct ir_inst_t inst[];
};

// initalize an ir block
struct ir_block_t *ir_init(void *start, void *end);

// immediate
struct ir_inst_t *ir_imm    (struct ir_block_t *, int32_t imm);
// register
struct ir_inst_t *ir_ld_reg (struct ir_block_t *, int32_t offset);
struct ir_inst_t *ir_st_reg (struct ir_block_t *, int32_t offset, struct ir_inst_t *val);
// branch and jump
struct ir_inst_t *ir_st_pc  (struct ir_block_t *, struct ir_inst_t *val);
struct ir_inst_t *ir_branch (struct ir_block_t *,
                             struct ir_inst_t *cond,
                             struct ir_inst_t *taken,
                             struct ir_inst_t *not_taken);
// alu
struct ir_inst_t *ir_add    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_sub    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_and    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_or     (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_xor    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_shr    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_sar    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_shl    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_mul    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_mulh   (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_mulhsu (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_mulhu  (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_div    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_divu   (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_rem    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_remu   (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);

// compare
struct ir_inst_t *ir_eq     (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_neq    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_lt     (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_ge     (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_ltu    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_geu    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
// store
struct ir_inst_t *ir_sb     (struct ir_block_t *, struct ir_inst_t *addr, struct ir_inst_t *val);
struct ir_inst_t *ir_sh     (struct ir_block_t *, struct ir_inst_t *addr, struct ir_inst_t *val);
struct ir_inst_t *ir_sw     (struct ir_block_t *, struct ir_inst_t *addr, struct ir_inst_t *val);
// load
struct ir_inst_t *ir_lb     (struct ir_block_t *, struct ir_inst_t *addr);
struct ir_inst_t *ir_lh     (struct ir_block_t *, struct ir_inst_t *addr);
struct ir_inst_t *ir_lw     (struct ir_block_t *, struct ir_inst_t *addr);
struct ir_inst_t *ir_lbu    (struct ir_block_t *, struct ir_inst_t *addr);
struct ir_inst_t *ir_lhu    (struct ir_block_t *, struct ir_inst_t *addr);
// system
struct ir_inst_t *ir_ecall  (struct ir_block_t *);
struct ir_inst_t *ir_ebreak (struct ir_block_t *);


void ir_eval(struct ir_block_t *, struct riscv_t *);
