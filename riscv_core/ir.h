#pragma once
#include <stdint.h>

#define IR_MAX_INST 256

struct ir_inst_t {
  int32_t op;
  // immediate / offset
  union {
    int32_t imm;
    int32_t offset;
  };
  // lhs / value
  union {
    struct ir_inst_t *lhs;
    struct ir_inst_t *value;
  };
  // rhs
  struct ir_inst_t *rhs;
  // parent node
  struct ir_inst_t *parent;
};

struct ir_block_t {
  int32_t head;
  // anything with a NULL parent is a root node to be emitted
  struct ir_inst_t inst[IR_MAX_INST];
};

void ir_init(struct ir_block_t *block);

struct ir_inst_t *ir_imm    (struct ir_block_t *, int32_t imm);
struct ir_inst_t *ir_ld_reg (struct ir_block_t *, int32_t offset);
struct ir_inst_t *ir_st_reg (struct ir_block_t *, int32_t offset, struct ir_inst_t *val);
struct ir_inst_t *ir_add    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_sub    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_and    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_or     (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_xor    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_sltu   (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_slt    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_shl    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_sal    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_sll    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_mul    (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
struct ir_inst_t *ir_imul   (struct ir_block_t *, struct ir_inst_t *lhs, struct ir_inst_t *rhs);
