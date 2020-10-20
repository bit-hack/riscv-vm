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

#include "decode.h"


// note: one idea would be to keep track of how often a block is hit and then
//       when inserting it into the hash map, we swap the hot block closer to
//       its ideal hash location.  it will then have a faster lookup.

// total size of the code block
static const uint32_t code_size = 1024 * 1024 * 8;

// total number of block map entries
static const uint32_t map_size = 1024 * 4;

static void rv_jit_clear(struct riscv_t *rv);


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

static void sys_free_exec_mem(void *ptr) {
#ifdef _WIN32
  VirtualFree(ptr, 0, MEM_RELEASE);
#endif
#ifdef __linux__
  assert(!"todo");
#endif
}

// this hash function is used when mapping addresses to indexes in the block map
static uint32_t wang_hash(uint32_t a) {
  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * 0x27d4eb2d;
  a = a ^ (a >> 15);
  return a;
}

// allocate a block map
void block_map_alloc(struct block_map_t *map, uint32_t max_entries) {
  assert(0 == (max_entries & (max_entries - 1)));
  const uint32_t size = max_entries * sizeof(struct block_t *);
  void *ptr = malloc(size);
  memset(ptr, 0, size);
  map->map = (struct block_t**)ptr;
  map->max_entries = max_entries;
  map->fill = 0;
}

// free a block map
static void block_map_free(struct block_map_t *map) {
  assert(map->map);
  free(map->map);
  map->map = NULL;
}

// clear all entries in the block map
// note: will not clear the code buffer
static void block_map_clear(struct block_map_t *map) {
  assert(map);
  memset(map->map, 0, map->max_entries * sizeof(struct block_t *));
  map->fill = 0;
}

// insert a block into a blockmap
static void block_map_insert(struct block_map_t *map, struct block_t *block) {
  assert(map->map && block);
  // insert into the block map
  const uint32_t mask = map->max_entries - 1;
  uint32_t index = wang_hash(block->pc_start);
  for (;; ++index) {
    if (map->map[index & mask] == NULL) {
      map->map[index & mask] = block;
      break;
    }
  }
  ++map->fill;
}

// expand the block map by a factor of x2
static void block_map_enlarge(struct riscv_jit_t *jit) {
  // allocate a new block map
  struct block_map_t new_map;
  block_map_alloc(&new_map, jit->block_map.max_entries * 2);
  // insert blocks into new map
  for (uint32_t i = 0; i < jit->block_map.max_entries; ++i) {
    struct block_t *block = jit->block_map.map[i];
    if (block) {
      block_map_insert(&new_map, block);
    }
  }
  // release old map
  block_map_free(&jit->block_map);
  // use new map
  jit->block_map.map = new_map.map;
  jit->block_map.max_entries = new_map.max_entries;
  // .fill remains unchanged
}

// allocate a new code block
static struct block_t *block_alloc(struct riscv_jit_t *jit) {
  // place a new block
  struct block_t *block = (struct block_t *)jit->code.head;
  struct cg_state_t *cg = &block->cg;
  // set the initial codegen write head
  cg_init(cg, block->code, jit->code.end);
  block->predict = NULL;
#if RISCV_JIT_PROFILE
  block->hit_count = 0;
#endif
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
  assert(jit && block && jit->code.head && jit->block_map.map);
  struct cg_state_t *cg = &block->cg;
  // advance the block head ready for the next alloc
  jit->code.head = block->code + cg_size(cg);
  // insert into the block map
  block_map_insert(&jit->block_map, block);

  // expand the block map when needed
  if (jit->block_map.fill * 2 > jit->block_map.max_entries) {
    block_map_enlarge(jit);
  }

#if RISCV_DUMP_JIT_TRACE
  block_dump(block, stdout);
#endif
  // flush the instructon cache for this block
  sys_flush_icache(block->code, cg_size(cg));
}

// try to locate an already translated block in the block map
static struct block_t *block_find(struct riscv_jit_t *jit, uint32_t addr) {
  assert(jit && jit->block_map.map);
  uint32_t index = wang_hash(addr);
  const uint32_t mask = jit->block_map.max_entries - 1;
  for (;; ++index) {
    struct block_t *block = jit->block_map.map[index & mask];
    if (block == NULL) {
      return NULL;
    }
    if (block->pc_start == addr) {
      return block;
    }
  }
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

static void handle_op_system(struct riscv_t *rv, uint32_t inst) {
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

#if 1
static void rv_translate_block(struct riscv_t *rv, struct block_t *block) {
  assert(rv && block);

  struct cg_state_t *cg = &block->cg;

  // setup the basic block
  block->instructions = 0;
  block->pc_start = rv->PC;
  block->pc_end = rv->PC;

  // prologue
  codegen_prologue(cg, false);

  // translate the basic block
  for (;;) {
    // fetch the next instruction
    const uint32_t inst = rv->io.mem_ifetch(rv, block->pc_end);
    // decode
    struct rv_inst_t dec;
    uint32_t pc = block->pc_end;
    if (!decode(inst, &dec, &pc)) {
      assert(!"unreachable");
    }
    // codegen
    if (!codegen(&dec, cg, block->pc_end, inst)) {
      assert(!"unreachable");
    }
    ++block->instructions;
    block->pc_end = pc;
    // stop on branch
    if (inst_is_branch(&dec)) {
      break;
    }
  }

  // epilogue
  codegen_epilogue(cg, false);
}
#else
static void rv_translate_block(struct riscv_t *rv, struct block_t *block) {
  assert(rv && block);

  struct cg_state_t *cg = &block->cg;

  // setup the basic block
  block->instructions = 0;
  block->pc_start = rv->PC;
  block->pc_end = rv->PC;

  uint32_t         lst_inst[1024];
  struct rv_inst_t lst_dec [1024];
  uint32_t         lst_pc  [1024];

  bool non_leaf = false;

  // decode the basic block
  uint32_t pc = block->pc_start;
  uint32_t num_decoded = 0;
  for (;;) {
    assert(num_decoded < 1024);
    // fetch the next instruction
    lst_pc[num_decoded] = pc;
    const uint32_t inst = rv->io.mem_ifetch(rv, pc);
    lst_inst[num_decoded] = inst;
    // decode an instruction
    struct rv_inst_t *dec = lst_dec + (num_decoded++);
    if (!decode(inst, dec, &pc)) {
      assert(!"unreachable");
    }
    // track leaf status
    non_leaf |= inst_will_call(dec);
    // stop on branch
    if (inst_is_branch(dec)) {
      break;
    }
  }

  // prologue
  codegen_prologue(cg, !non_leaf);
  // codegen all decoded instructions
  for (uint32_t i = 0; i < num_decoded; ++i) {
    // information we need
    const uint32_t inst = lst_inst[i];
    struct rv_inst_t *dec = lst_dec + i;
    // codegen
    if (!codegen(dec, cg, lst_pc[i], inst)) {
      assert(!"unreachable");
    }
  }
  // epilogue
  codegen_epilogue(cg, !non_leaf);

  block->instructions = num_decoded;
  block->pc_end = pc;
}
#endif

static struct block_t *block_find_or_translate(struct riscv_t *rv, struct block_t *prev) {
  // lookup the next block in the block map
  struct block_t *next = block_find(&rv->jit, rv->PC);
  // translate if we didnt find one
  if (!next) {

    static const uint32_t margin = 1024;
    if (rv->jit.code.head + margin > rv->jit.code.end) {
      rv_jit_clear(rv);
      prev = NULL;
    }

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

static void rv_jit_dump_stats(struct riscv_t *rv) {
  struct riscv_jit_t *jit = &rv->jit;

  uint32_t num_blocks = 0;
  uint32_t code_size = (uint32_t)(jit->code.head - jit->code.start);

  for (uint32_t i = 0; i < jit->block_map.max_entries; ++i) {
    struct block_t *block = jit->block_map.map[i];
    if (!block) {
      continue;
    }
    ++num_blocks;

#if RISCV_JIT_PROFILE
    if (block->hit_count > 1000) {
      block_dump(block, stdout);
      fprintf(stdout, "Hit count: %u\n", block->hit_count);
    }
#endif
  }

  fprintf(stdout, "Number of blocks: %u\n", num_blocks);
  fprintf(stdout, "Code size: %u\n", code_size);
}

// flush the blockmap and code cache
static void rv_jit_clear(struct riscv_t *rv) {
  struct riscv_jit_t *jit = &rv->jit;
  // clear the block map
  block_map_clear(&jit->block_map);
  // reset the code buffer write position
  jit->code.head = jit->code.start;
}

void rv_step(struct riscv_t *rv, int32_t cycles) {

  // find or translate a block for our starting PC
  struct block_t *block = block_find_or_translate(rv, NULL);
  assert(block);

  const uint64_t cycles_start = rv->csr_cycle;
  const uint64_t cycles_target = rv->csr_cycle + cycles;

  // loop until we hit out cycle target
  while (rv->csr_cycle < cycles_target && !rv->halt) {

    const uint32_t pc = rv->PC;

    // try to predict the next block
    // note: block predition gives us ~100 MIPS boost.
    if (block->predict && block->predict->pc_start == pc) {
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
#if RISCV_JIT_PROFILE
    block->hit_count++;
#endif
    call_block_t c = (call_block_t)block->code;
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

bool rv_jit_init(struct riscv_t *rv) {
  struct riscv_jit_t *jit = &rv->jit;

  // allocate the block map which maps address to blocks
  if (jit->block_map.map == NULL) {
    jit->block_map.max_entries = map_size;
    block_map_alloc(&jit->block_map, map_size);
  }

  // allocate block/code storage space
  if (jit->code.start == NULL) {
    void *ptr = sys_alloc_exec_mem(code_size);
    memset(ptr, 0xcc, code_size);
    jit->code.start = ptr;
    jit->code.head = ptr;
    jit->code.end = jit->code.start + code_size;
  }

  // setup nonjit instruction callbacks
  jit->handle_op_op     = handle_op_op;
  jit->handle_op_fp     = handle_op_fp;
  jit->handle_op_system = handle_op_system;

  return true;
}

void rv_jit_free(struct riscv_t *rv) {
  struct riscv_jit_t *jit = &rv->jit;

#if RISCV_DUMP_JIT_BLOCK
  rv_jit_dump_stats(rv);
#endif

  if (jit->block_map.map) {
    block_map_free(&jit->block_map);
    jit->block_map.map = NULL;
  }

  if (jit->code.start) {
    sys_free_exec_mem(jit->code.start);
    jit->code.start = NULL;
  }
}
