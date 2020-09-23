#include <cstring>
#include <memory>

#include "elf.h"
#include "file.h"
#include "memory.h"

#include "../riscv_core/riscv.h"


// any `ecall` should halt the program (for compliance testing)
static const bool DO_COMPLIANCE_CONFIG = false;

// emit an execution signature (for compliance testing)
static const bool DO_SIGNATURE = false;

// emit an instruction trace
static const bool DO_TRACE = false;


// define the valid memory region for sbrk
enum {
  sbrk_start = 0x10000000,
  sbrk_end   = 0x1fffffff,
};


namespace {

// state structure passed to the VM
struct state_t {
  memory_t mem;
  bool done;
  // the .data break address
  riscv_word_t break_addr;
};

// newlib _write syscall handler
void syscall_write(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // access parameters
  riscv_word_t handle = 0;
  riscv_word_t buffer = 0;
  riscv_word_t count = 0;
  rv_get_reg(rv, rv_reg_a0, &handle);
  rv_get_reg(rv, rv_reg_a1, &buffer);
  rv_get_reg(rv, rv_reg_a2, &count);
  // read the string that we are printing
  std::array<char, 128> temp;
  uint32_t size = std::min(count, (uint32_t)temp.size() - 1);
  s->mem.read((uint8_t*)temp.data(), buffer, size);
  // enforce trailing end of string
  temp[size] = '\0';
  // print out the string
  fprintf(stdout, "%s", temp.data());
  // return number of bytes written
  rv_set_reg(rv, rv_reg_a0, size);
}

// newlib _exit syscall handler
void syscall_exit(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  s->done = true;
  // get the exit code
  riscv_word_t exit_code = 0;
  rv_get_reg(rv, rv_reg_a0, &exit_code);
  fprintf(stdout, "inferior exit code %d\n", (int)exit_code);
}

// newlib _sbrk syscall handler
void syscall_sbrk(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // get the increment parameter
  riscv_word_t increment = 0;
  rv_get_reg(rv, rv_reg_a0, &increment);
  // increment the break pointer
  if (increment) {
    if (increment <= sbrk_start || increment > sbrk_end) {
      rv_set_reg(rv, rv_reg_a0, s->break_addr);
      return;
    }
  }
  // return the old break address
  rv_set_reg(rv, rv_reg_a0, s->break_addr);
  // store the new break address
  if (increment) {
    s->break_addr = increment;
  }
}

riscv_word_t imp_mem_read_w(struct riscv_t *rv, riscv_word_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  uint32_t out = 0;
  s->mem.read((uint8_t*)&out, addr, sizeof(out));
  return out;
}

riscv_half_t imp_mem_read_s(struct riscv_t *rv, riscv_word_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  uint16_t out = 0;
  s->mem.read((uint8_t*)&out, addr, sizeof(out));
  return out;
}

riscv_byte_t imp_mem_read_b(struct riscv_t *rv, riscv_word_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  uint8_t out = 0;
  s->mem.read((uint8_t*)&out, addr, sizeof(out));
  return out;
}

void imp_mem_write_w(struct riscv_t *rv, riscv_word_t addr, riscv_word_t data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_mem_write_s(struct riscv_t *rv, riscv_word_t addr, riscv_half_t data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_mem_write_b(struct riscv_t *rv, riscv_word_t addr, riscv_byte_t  data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_on_ecall(struct riscv_t *rv, riscv_word_t addr, uint32_t inst) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);

  if (DO_COMPLIANCE_CONFIG) {
    // in compliance testing it seems any `ecall` should abort
    s->done = true;
  }

  // get the syscall number
  riscv_word_t syscall = 0;
  rv_get_reg(rv, rv_reg_a7, &syscall);
  // dispatch call type
  switch (syscall) {
  case 57:  // _close
  case 62:  // _lseek
  case 63:  // _read
    break;
  case 64:  // _write
    syscall_write(rv);
    break;
  case 80:  // _fstat
    break;
  case 214: // _sbrk
    syscall_sbrk(rv);
    break;
  case 93:  // exit
    syscall_exit(rv);
    break;
  default:
    fprintf(stderr, "unknown syscall %d\n", int(syscall));
    s->done = true;
    break;
  }
}

void imp_on_ebreak(struct riscv_t *rv, riscv_word_t addr, uint32_t inst) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->done = true;
}

} // namespace {}

int main(int argc, char **args) {

  if (argc <= 1) {
    fprintf(stderr, "Usage: %s program.elf\n", args[0]);
    return 1;
  }

  // load the ELF file from disk
  elf_t elf;
  if (!elf.load(args[1])) {
    fprintf(stderr, "Unable to load ELF file '%s'\n", args[1]);
    return 1;
  }

  // setup the IO handlers for the VM
  const riscv_io_t io = {
    imp_mem_read_w,
    imp_mem_read_s,
    imp_mem_read_b,
    imp_mem_write_w,
    imp_mem_write_s,
    imp_mem_write_b,
    imp_on_ecall,
    imp_on_ebreak,
  };

  auto state = std::make_unique<state_t>();
  state->break_addr = sbrk_start;

  // create the VM
  riscv_t *rv = rv_create(&io, state.get());
  if (!rv) {
    fprintf(stderr, "Unable to create riscv emulator\n");
    return 1;
  }

  // upload the ELF file into our memory abstraction
  if (!elf.upload(rv, state->mem)) {
    fprintf(stderr, "Unable to upload ELF file '%s'\n", args[1]);
    return 1;
  }

  const int max_cycles = 1000000;

  // run until we hit max_cycles or flag that we are done
  for (int i = 0; !state->done && i < max_cycles; ++i) {
    // trace execution
    if (DO_TRACE) {
      uint32_t pc = 0;
      rv_get_pc(rv, &pc);
      printf("%08x\n", pc);
    }
    // single step instructions
    rv_step(rv);
  }

  // print execution signature
  if (DO_SIGNATURE) {
    uint32_t start = 0, end = 0;
    if (elf.get_data_section_range(start, end)) {
      // try and access the exact signature start
      if (const ELF::Elf32_Sym *sym = elf.get_symbol("begin_signature")) {
        start = sym->st_value;
      }
      // dump the data
      uint32_t value = 0;
      for (uint32_t i = start; i <= end; i += 4) {
        state->mem.read((uint8_t*)&value, i, 4);
        printf("%08x\n", value);
      }
    }
  }

  // delete the VM
  rv_delete(rv);
  return 0;
}
