#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  rv_reg_zero,
  rv_reg_ra,
  rv_reg_sp,
  rv_reg_gp,
  rv_reg_tp,
  rv_reg_t0,
  rv_reg_t1,
  rv_reg_t2,
  rv_reg_s0,
  rv_reg_s1,
  rv_reg_a0,
  rv_reg_a1,
  rv_reg_a2,
  rv_reg_a3,
  rv_reg_a4,
  rv_reg_a5,
  rv_reg_a6,
  rv_reg_a7,
  rv_reg_s2,
  rv_reg_s3,
  rv_reg_s4,
  rv_reg_s5,
  rv_reg_s6,
  rv_reg_s7,
  rv_reg_s8,
  rv_reg_s9,
  rv_reg_s10,
  rv_reg_s11,
  rv_reg_t3,
  rv_reg_t4,
  rv_reg_t5,
  rv_reg_t6,
};

struct riscv_t;

typedef uint32_t (*riscv_mem_read_w)(struct riscv_t *rv, uint32_t addr);
typedef uint16_t (*riscv_mem_read_s)(struct riscv_t *rv, uint32_t addr);
typedef uint8_t  (*riscv_mem_read_b)(struct riscv_t *rv, uint32_t addr);

typedef void (*riscv_mem_write_w)(struct riscv_t *rv, uint32_t addr, uint32_t data);
typedef void (*riscv_mem_write_s)(struct riscv_t *rv, uint32_t addr, uint16_t data);
typedef void (*riscv_mem_write_b)(struct riscv_t *rv, uint32_t addr, uint8_t  data);

struct riscv_io_t {
  // memory read interface
  riscv_mem_read_w mem_read_w;
  riscv_mem_read_s mem_read_s;
  riscv_mem_read_b mem_read_b;
  // memory write interface
  riscv_mem_write_w mem_write_w;
  riscv_mem_write_s mem_write_s;
  riscv_mem_write_b mem_write_b;
};

struct riscv_t *rv_create(const struct riscv_io_t *io, void *userdata);

void rv_delete(struct riscv_t *);

void rv_reset(struct riscv_t *);

void rv_step(struct riscv_t *);

void *rv_userdata(struct riscv_t *);

void rv_set_pc(struct riscv_t *rv, uint32_t pc);

void rv_get_pc(struct riscv_t *rv, uint32_t *out);

void rv_set_reg(struct riscv_t *, uint32_t reg, uint32_t in);

void rv_get_reg(struct riscv_t *, uint32_t reg, uint32_t *out);

#ifdef __cplusplus
};  // ifdef __cplusplus
#endif
