#include <memory>
#include <stdio.h>
#include <stdlib.h>

struct file_t {

  bool load(const char *path) {

    if (mem) {
      unload();
    }

    FILE *fd = fopen(path, "rb");
    if (!fd) {
      return false;
    }

    fseek(fd, 0, SEEK_END);
    mem_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    if (mem_size == 0) {
      fclose(fd);
      return false;
    }
    mem.reset(new uint8_t[mem_size]);

    const size_t read = fread(mem.get(), 1, mem_size, fd);

    fclose(fd);
    if (read != mem_size) {
      unload();
      return false;
    }

    return true;
  }

  void unload() {
    if (mem) {
      mem.reset();
    }
    mem_size = 0;
  }

  uint8_t *data() const { return mem.get(); }

  size_t size() const { return mem_size; }

protected:
  std::unique_ptr<uint8_t[]> mem;
  size_t mem_size;
};
