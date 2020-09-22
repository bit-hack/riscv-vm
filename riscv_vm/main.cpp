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


namespace {

// state structure passed to the VM
struct state_t {
  memory_t mem;
  bool done;
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
  printf("%s", temp.data());
  // return number of bytes written
  rv_set_reg(rv, rv_reg_a0, size);
}

// newlib _exit syscall handler
void syscall_exit(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  s->done = true;
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
  case 214: // _sbrk
    break;
  case 93:  // exit
    syscall_exit(rv);
    break;
  default:
    s->done = true;
    break;
  }
}

void imp_on_ebreak(struct riscv_t *rv, riscv_word_t addr, uint32_t inst) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->done = true;
}

// a very minimal ELF parser
struct elf_t {

  elf_t(file_t &elf_file)
    : file(elf_file)
    , data(file.data())
    , hdr((Elf32_Ehdr*)data)
  {
  }

  // check the ELF file header is valid
  bool is_valid() const {
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
    // success
    return true;
  }

  // get section header string table
  const char *get_sh_string(int index) const {
    const Elf32_Shdr *shdr = (const Elf32_Shdr*)(data + hdr->e_shoff + hdr->e_shstrndx * hdr->e_shentsize);
    return (const char*)(data + shdr->sh_offset + index);
  }

  // get a section header
  const Elf32_Shdr *get_section_header(const char *name) const {
    for (int s = 0; s < hdr->e_shnum; ++s) {
      const Elf32_Shdr *shdr = (const Elf32_Shdr*)(data + hdr->e_shoff + (s * hdr->e_shentsize));
      const char *sname = get_sh_string(shdr->sh_name);
      if (strcmp(name, sname) == 0) {
        return shdr;
      }
    }
    return nullptr;
  }

  // get the load range of a section
  bool get_data_section_range(uint32_t &start, uint32_t &end) const {
    const Elf32_Shdr *shdr = get_section_header(".data");
    if (!shdr) {
      return false;
    }
    if (shdr->sh_type == SHT_NOBITS) {
      return false;
    }
    start = shdr->sh_addr;
    end = start + shdr->sh_size;
    return true;
  }

  // get the ELF string table
  const char *get_strtab() const {
    const Elf32_Shdr *shdr = get_section_header(".strtab");
    if (!shdr) {
      return nullptr;
    }
    return (const char *)(data + shdr->sh_offset);
  }

  // find a symbol entry
  const Elf32_Sym* get_symbol(const char *name) const {
    // get the string table
    const char *strtab = get_strtab();
    if (!strtab) {
      return nullptr;
    }
    // get the symbol table
    const Elf32_Shdr *shdr = get_section_header(".symtab");
    if (!shdr) {
      return nullptr;
    }
    // find symbol table range
    const Elf32_Sym *sym = (const Elf32_Sym *)(data + shdr->sh_offset);
    const Elf32_Sym *end = (const Elf32_Sym *)(data + shdr->sh_offset + shdr->sh_size);
    // try to find the symbol
    for (; sym < end; ++sym) {
      const char *sym_name = strtab + sym->st_name;
      if (strcmp(name, sym_name) == 0) {
        return sym;
      }
    }
    // cant find the symbol
    return nullptr;
  }

  // load the ELF file into a memory abstraction
  bool upload(struct riscv_t *rv, memory_t &mem) const {
    // set the entry point
    rv_set_pc(rv, hdr->e_entry);
    // loop over all of the program headers
    for (int p = 0; p < hdr->e_phnum; ++p) {
      // find next program header
      const Elf32_Phdr *phdr = (const Elf32_Phdr*)(data + hdr->e_phoff + (p * hdr->e_phentsize));
      // check this section should be loaded
      if (phdr->p_type != PT_LOAD) {
        continue;
      }
      // memcpy required range
      const int to_copy = std::min(phdr->p_memsz, phdr->p_filesz);
      if (to_copy) {
        mem.write(phdr->p_vaddr, data + phdr->p_offset, to_copy);
      }
      // zero fill required range
      const int to_zero = std::max(phdr->p_memsz, phdr->p_filesz) - to_copy;
      if (to_zero) {
        mem.fill(phdr->p_vaddr + to_copy, to_zero, 0);
      }
    }
    // success
    return true;
  }

protected:
  file_t &file;
  const uint8_t *data;
  Elf32_Ehdr *hdr;
};

} // namespace {}

int main(int argc, char **args) {

  if (argc <= 1) {
    fprintf(stderr, "Usage: %s program.elf\n", args[0]);
    return 1;
  }

  // load the ELF file into host memory
  file_t elf_file;
  if (!elf_file.load(args[1])) {
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

  // create the VM
  riscv_t *rv = rv_create(&io, state.get());
  if (!rv) {
    fprintf(stderr, "Unable to create riscv emulator\n");
    return 1;
  }

  // check this is a valid ELF file
  elf_t elf{ elf_file };
  if (!elf.is_valid()) {
    fprintf(stderr, "Invalid ELF file '%s'\n", args[1]);
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
      if (const Elf32_Sym *sym = elf.get_symbol("begin_signature")) {
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
