#include <memory>

#include "elf.h"
#include "file.h"
#include "memory.h"

#include "../riscv_emu/riscv.h"


// emit an instruction trace
#define DO_TRACE      0

// emit an execution signature
#define DO_SIGNATURE  1


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
}

void imp_on_ebreak(struct riscv_t *rv, uint32_t addr, uint32_t inst) {
  state_t *s = (state_t*)rv_userdata(rv);
  s->done = true;
}

// a very minimal ELF parser
struct elf_t {

  elf_t(file_t &file)
    : _file(file)
    , _data(_file.data())
    , _hdr((Elf32_Ehdr*)_data)
  {
  }

  bool is_valid() const {
    // check for ELF magic
    if (_hdr->e_ident[0] != 0x7f &&
        _hdr->e_ident[1] != 'E' &&
        _hdr->e_ident[2] != 'L' &&
        _hdr->e_ident[3] != 'F') {
      return false;
    }
    // must be 32bit ELF
    if (_hdr->e_ident[EI_CLASS] != ELFCLASS32) {
      return false;
    }
    // check machine type is RISCV
    if (_hdr->e_machine != EM_RISCV) {
      return false;
    }
    // success
    return true;
  }

  // get section header string table
  const char *get_sh_string(int index) const {
    const Elf32_Shdr *shdr = (const Elf32_Shdr*)(_data + _hdr->e_shoff + _hdr->e_shstrndx * _hdr->e_shentsize);
    return (const char*)(_data + shdr->sh_offset + index);
  }

  // get a section header
  const Elf32_Shdr *get_section_header(char *name) const {
    for (int s = 0; s < _hdr->e_shnum; ++s) {
      const Elf32_Shdr *shdr = (const Elf32_Shdr*)(_data + _hdr->e_shoff + (s * _hdr->e_shentsize));
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
    return (const char *)(_data + shdr->sh_offset);
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
    const Elf32_Sym *sym = (const Elf32_Sym *)(_data + shdr->sh_offset);
    const Elf32_Sym *end = (const Elf32_Sym *)(_data + shdr->sh_offset + shdr->sh_size);
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

  bool upload(struct riscv_t *rv, memory_t &mem) const {
    // set the entry point
    rv_set_pc(rv, _hdr->e_entry);
    // loop over all of the program headers
    for (int p = 0; p < _hdr->e_phnum; ++p) {
      // find next program header
      const Elf32_Phdr *phdr = (const Elf32_Phdr*)(_data + _hdr->e_phoff + (p * _hdr->e_phentsize));
      // check this section should be loaded
      if (phdr->p_type != PT_LOAD) {
        continue;
      }
      // memcpy required range
      const int to_copy = std::min(phdr->p_memsz, phdr->p_filesz);
      if (to_copy) {
        mem.write(phdr->p_vaddr, _data + phdr->p_offset, to_copy);
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
  file_t &_file;
  const uint8_t *_data;
  Elf32_Ehdr *_hdr;
};

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

  elf_t elf{ elf_file };
  if (!elf.is_valid()) {
    fprintf(stderr, "Invalid ELF file '%s'\n", args[1]);
    return 1;
  }

  if (!elf.upload(rv, state->mem)) {
    fprintf(stderr, "Unable to upload ELF file '%s'\n", args[1]);
    return 1;
  }

  const int max_cycles = 10000;

  for (int i = 0; !state->done && i < max_cycles; ++i) {

#if DO_TRACE
    // trace execution
    uint32_t pc = 0;
    rv_get_pc(rv, &pc);
    printf("%08lx\n", pc);
#endif

    // single step instructions
    rv_step(rv);
  }

#if DO_SIGNATURE
  // print signature (contents of .data section)
  {
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
#endif

  rv_delete(rv);
  return 0;
}
