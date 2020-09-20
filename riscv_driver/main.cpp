#include <memory>

#include "elf.h"
#include "file.h"
#include "memory.h"

#include "../riscv_emu/riscv.h"


namespace {

struct state_t {
  memory_t mem;
  bool done;
};

uint32_t imp_mem_read_w(struct riscv_t *rv, uint32_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  uint32_t out = 0;
  s->mem.read((uint8_t*)&out, addr, sizeof(out));
  return out;
}

uint16_t imp_mem_read_s(struct riscv_t *rv, uint32_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  uint16_t out = 0;
  s->mem.read((uint8_t*)&out, addr, sizeof(out));
  return out;
}

uint8_t imp_mem_read_b(struct riscv_t *rv, uint32_t addr) {
  state_t *s = (state_t*)rv_userdata(rv);
  uint8_t out = 0;
  s->mem.read((uint8_t*)&out, addr, sizeof(out));
  return out;
}

void imp_mem_write_w(struct riscv_t *rv, uint32_t addr, uint32_t data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_mem_write_s(struct riscv_t *rv, uint32_t addr, uint16_t data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_mem_write_b(struct riscv_t *rv, uint32_t addr, uint8_t  data) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->mem.write(addr, (uint8_t*)&data, sizeof(data));
}

void imp_on_ecall(struct riscv_t *rv, uint32_t addr, uint32_t inst) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->done = true;

  uint32_t a0 = 0;
  rv_get_reg(rv, rv_reg_a0, &a0);
  printf("ecall (a0 = %u)\n", a0);
}

void imp_on_ebreak(struct riscv_t *rv, uint32_t addr, uint32_t inst) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->done = true;
}

bool load_elf(struct riscv_t *rv, memory_t &mem, file_t &file) {

  const uint8_t *ptr = file.data();
  Elf32_Ehdr *hdr = (Elf32_Ehdr*)ptr;

  // check for ELF magic
  if (hdr->e_ident[0] != 0x7f &&
      hdr->e_ident[1] != 'E' &&
      hdr->e_ident[2] != 'L' &&
      hdr->e_ident[3] != 'F') {
    return false;
  }
  // must be 32bit ELF
  if (hdr->e_ident[EI_CLASS] != ELFCLASS32) {
    return false;
  }
  // check machine type is RISCV
  if (hdr->e_machine != EM_RISCV) {
    return false;
  }

  // set the entry point
  rv_set_pc(rv, hdr->e_entry);

  // loop over all of the program headers
  for (int p = 0; p < hdr->e_phnum; ++p) {

    const Elf32_Phdr *phdr = (const Elf32_Phdr*)(ptr + hdr->e_phoff + (p * hdr->e_phentsize));

    if (phdr->p_type != PT_LOAD) {
      continue;
    }

    const int to_copy = std::min(phdr->p_memsz, phdr->p_filesz);
    mem.write(phdr->p_vaddr, ptr + phdr->p_offset, to_copy);

    const int to_zero = std::max(phdr->p_memsz, phdr->p_filesz) - to_copy;
    mem.fill(phdr->p_vaddr + to_copy, to_zero, 0);
  }

  return true;
}

} // namespace {}


int main(int argc, char **args) {

  if (argc <= 1) {
    fprintf(stderr, "Usage: %s program.elf\n", args[0]);
    return 1;
  }

  file_t elf_file;
  if (!elf_file.load(args[1])) {
    fprintf(stderr, "Unable to load ELF file '%s'\n", args[1]);
    return 1;
  }

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

  riscv_t *rv = rv_create(&io, state.get());
  if (!rv) {
    fprintf(stderr, "Unable to create riscv emulator\n");
    return 1;
  }

  if (!load_elf(rv, state->mem, elf_file)) {
    fprintf(stderr, "Unable to parse ELF file '%s'\n", args[1]);
    return 1;
  }

  const int max_cycles = 10000;

  for (int i = 0; !state->done && i < max_cycles; ++i) {

#if 0
    // trace execution
    uint32_t pc = 0;
    rv_get_pc(rv, &pc);
    printf("%08lx\n", pc);
#endif

    // single step instructions
    rv_step(rv);
  }

  // print signature
  std::array<uint32_t, 36> sig;
  state->mem.read((uint8_t*)sig.data(), 0x00011320, sig.size() * 4);
  for (const uint32_t d : sig) {
    printf("%08x\n", d);
  }

  rv_delete(rv);
  return 0;
}
