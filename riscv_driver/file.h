#include <memory>
#include <stdio.h>
#include <stdlib.h>

struct file_t {

  bool load(const char *path) {

    if (_data) {
      unload();
    }

    FILE *fd = fopen(path, "rb");
    if (!fd) {
      return false;
    }

    fseek(fd, 0, SEEK_END);
    _size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    if (_size == 0) {
      fclose(fd);
      return false;
    }
    _data.reset(new uint8_t[_size]);

    const size_t read = fread(_data.get(), 1, _size, fd);

    fclose(fd);
    if (read != _size) {
      unload();
      return false;
    }

    return true;
  }

  void unload() {
    if (_data) {
      _data.reset();
    }
    _size = 0;
  }

  uint8_t *data() const { return _data.get(); }

  size_t size() const { return _size; }

protected:
  std::unique_ptr<uint8_t[]> _data;
  size_t _size;
};
