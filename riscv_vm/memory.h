#pragma once
#include <cstdint>
#include <array>


struct memory_t {

  struct chunk_t {
    std::array<uint8_t, 0x10000> data;
  };

  memory_t() {
    chunks.fill(nullptr);
  }

  ~memory_t() {
    clear();
  }

  uint32_t read_str(uint8_t *dst, uint32_t addr, uint32_t max) {
    uint32_t len = 0;
    const uint8_t *end = dst + max;
    for (;; ++len, ++dst) {
      uint8_t ch = 0;
      read(&ch, addr + len, 1);
      if (dst < end) {
        *dst = ch;
      }
      if (ch == 0) {
        break;
      }
    }
    return len + 1;
  }

  void read(uint8_t *dst, uint32_t addr, uint32_t size) {
    for (uint32_t i=0; i<size; ++i) {
      uint32_t p = addr + i;
      chunk_t *c = chunks[p >> 16];
      dst[i] = c ? c->data[p & 0xffff] : 0;
    }
  }

  void write(uint32_t addr, const uint8_t *src, uint32_t size) {
    for (uint32_t i=0; i<size; ++i) {
      uint32_t p = addr + i;
      uint32_t x = p >> 16;
      chunk_t *c = chunks[x];
      if (c == nullptr) {
        c = new chunk_t;
        c->data.fill(0);
        chunks[x] = c;
      }
      c->data[p & 0xffff] = src[i];
    }
  }

  void fill(uint32_t addr, uint32_t size, uint8_t val) {
    for (uint32_t i = 0; i < size; ++i) {
      uint32_t p = addr + i;
      uint32_t x = p >> 16;
      chunk_t *c = chunks[x];
      if (c == nullptr) {
        c = new chunk_t;
        c->data.fill(0);
        chunks[x] = c;
      }
      c->data[p & 0xffff] = val;
    }
  }

  void clear() {
    for (chunk_t *c : chunks) {
      if (c) {
        delete c;
      }
    }
    chunks.fill(nullptr);
  }

protected:
  std::array<chunk_t*, 0x10000> chunks;
};
