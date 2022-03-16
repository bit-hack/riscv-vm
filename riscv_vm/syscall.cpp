#include <cstdint>
#include <cstdio>
#include <ctime>
#include "alloca.h"
#include "../riscv_core/riscv.h"
#include "state.h"

enum {
  SYS_getcwd = 17,
  SYS_dup = 23,
  SYS_fcntl = 25,
  SYS_faccessat = 48,
  SYS_chdir = 49,
  SYS_openat = 56,
  SYS_close = 57,
  SYS_getdents = 61,
  SYS_lseek = 62,
  SYS_read = 63,
  SYS_write = 64,
  SYS_writev = 66,
  SYS_pread = 67,
  SYS_pwrite = 68,
  SYS_fstatat = 79,
  SYS_fstat = 80,
  SYS_exit = 93,
  SYS_exit_group = 94,
  SYS_kill = 129,
  SYS_rt_sigaction = 134,
  SYS_times = 153,
  SYS_uname = 160,
  SYS_gettimeofday = 169,
  SYS_getpid = 172,
  SYS_getuid = 174,
  SYS_geteuid = 175,
  SYS_getgid = 176,
  SYS_getegid = 177,
  SYS_brk = 214,
  SYS_munmap = 215,
  SYS_mremap = 216,
  SYS_mmap = 222,
  SYS_open = 1024,
  SYS_link = 1025,
  SYS_unlink = 1026,
  SYS_mkdir = 1030,
  SYS_access = 1033,
  SYS_stat = 1038,
  SYS_lstat = 1039,
  SYS_time = 1062,
  SYS_getmainvars = 2011,
};

enum {
  O_RDONLY = 0,
  O_WRONLY = 1,
  O_RDWR = 2,
  O_ACCMODE = 3,
};

// from syscall_sdl.cpp
void syscall_draw_frame(struct riscv_t *rv);
void syscall_draw_frame_pal(struct riscv_t *rv);

namespace {

int find_free_fd(struct state_t *s) {
  for (int i = 3; ; ++i) {
    auto itt = s->fd_map.find(i);
    if (s->fd_map.end() == itt) {
      return i;
    }
  }
}

const char *get_mode_str(uint32_t flags, uint32_t mode) {
  switch (flags & O_ACCMODE) {
  case O_RDONLY:
    return "rb";
  case O_WRONLY:
    return "wb";
  case O_RDWR:
    return "a+";
  default:
    return nullptr;
  }
}

}  // namespace

void syscall_write(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // _write(handle, buffer, count)
  riscv_word_t handle = rv_get_reg(rv, rv_reg_a0);
  riscv_word_t buffer = rv_get_reg(rv, rv_reg_a1);
  riscv_word_t count  = rv_get_reg(rv, rv_reg_a2);
  // read the string that we are printing
  uint8_t *temp = (uint8_t*)alloca(count);
  s->mem.read((uint8_t*)temp, buffer, count);
  // lookup the file descriptor
  auto itt = s->fd_map.find(int(handle));
  if (itt != s->fd_map.end()) {
    // write out the data
    size_t written = fwrite(temp, 1, count, itt->second);
    // return number of bytes written
    rv_set_reg(rv, rv_reg_a0, (riscv_word_t)written);
  }
  else {
    // error
    rv_set_reg(rv, rv_reg_a0, -1);
  }
}

void syscall_exit(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  rv_halt(rv);
  // _exit(code);
  riscv_word_t code = rv_get_reg(rv, rv_reg_a0);
  fprintf(stdout, "inferior exit code %d\n", (int)code);
}

void syscall_brk(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // get the increment parameter
  riscv_word_t increment = rv_get_reg(rv, rv_reg_a0);
  // note: this doesnt seem to be how it should operate but checking the
  //       syscall source this is what it expects.
  //       the return value doesnt seem to be documented this way.
  if (increment) {
    s->break_addr = increment;
  }
  // return new break address
  rv_set_reg(rv, rv_reg_a0, s->break_addr);
}

void syscall_gettimeofday(struct riscv_t *rv) {
#define FIXED_TIME_STEP 0
#if FIXED_TIME_STEP
  static clock_t fake = 0;
#endif
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // get the parameters
  riscv_word_t tv = rv_get_reg(rv, rv_reg_a0);
  riscv_word_t tz = rv_get_reg(rv, rv_reg_a1);
  // return the clock time
  if (tv) {
#if FIXED_TIME_STEP
    clock_t t = (fake += CLOCKS_PER_SEC / 10);
#else
    clock_t t = clock();
#endif
    int32_t tv_sec = t / CLOCKS_PER_SEC;
    int32_t tv_usec = (t % CLOCKS_PER_SEC) * (1000000 / CLOCKS_PER_SEC);
    s->mem.write(tv + 0, (const uint8_t*)&tv_sec,  4);
    // note: I thought this was offset 4 (tv_sec is a long) but looking at the asm
    //       its at offset 8.  Even though it does just issue an lw to read it.
    s->mem.write(tv + 8, (const uint8_t*)&tv_usec, 4);
  }
  if (tz) {
    // note: This param is currently ignored by the syscall handler in newlib so it
    //       would be useless to do anything here.
  }
  // success
  rv_set_reg(rv, rv_reg_a0, 0);
}

void syscall_close(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // _close(fd);
  uint32_t fd = rv_get_reg(rv, rv_reg_a0);
  // lookup the file descriptor in question
  if (fd >= 3) {
    auto itt = s->fd_map.find(int(fd));
    if (itt != s->fd_map.end()) {
      fclose(itt->second);
      s->fd_map.erase(itt);
      // success
      rv_set_reg(rv, rv_reg_a0, 0);
    }
  }
  // success
  rv_set_reg(rv, rv_reg_a0, 0);
}

void syscall_lseek(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // _lseek(fd, offset, whence);
  uint32_t fd     = rv_get_reg(rv, rv_reg_a0);
  uint32_t offset = rv_get_reg(rv, rv_reg_a1);
  uint32_t whence = rv_get_reg(rv, rv_reg_a2);
  // find the file descriptor
  auto itt = s->fd_map.find(int(fd));
  if (itt == s->fd_map.end()) {
    // error
    rv_set_reg(rv, rv_reg_a0, -1);
    return;
  }
  FILE *handle = itt->second;
  // perform the seek
  // note: the whence defines seems somewhat portable and doesnt require some
  //       kind of mapping.
  if (fseek(handle, offset, whence)) {
    // error
    rv_set_reg(rv, rv_reg_a0, -1);
    return;
  }
  // success
  rv_set_reg(rv, rv_reg_a0, 0);
}

void syscall_read(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // _read(fd, buf, count);
  uint32_t fd    = rv_get_reg(rv, rv_reg_a0);
  uint32_t buf   = rv_get_reg(rv, rv_reg_a1);
  uint32_t count = rv_get_reg(rv, rv_reg_a2);
  // lookup the file
  auto itt = s->fd_map.find(int(fd));
  if (itt == s->fd_map.end()) {
    // error
    rv_set_reg(rv, rv_reg_a0, -1);
    return;
  }
  FILE *handle = itt->second;
  // read the file into VM memory
  uint8_t *temp = (uint8_t*)alloca(count);
  size_t read = fread(temp, 1, count, handle);
  s->mem.write(buf, temp, uint32_t(read));
  // success
  rv_set_reg(rv, rv_reg_a0, uint32_t(read));
}

void syscall_fstat(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // TODO
}

void syscall_open(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // _open(name, flags, mode);
  uint32_t name  = rv_get_reg(rv, rv_reg_a0);
  uint32_t flags = rv_get_reg(rv, rv_reg_a1);
  uint32_t mode  = rv_get_reg(rv, rv_reg_a2);
  // read name from VM memory
  std::array<char, 256> name_str = { '\0' };
  uint32_t read = s->mem.read_str((uint8_t*)name_str.data(), name, uint32_t(name_str.size()));
  if (read > name_str.size()) {
    rv_set_reg(rv, rv_reg_a0, -1);
    return;
  }
  // open the file
  const char *mode_str = get_mode_str(flags, mode);
  if (!mode_str) {
    rv_set_reg(rv, rv_reg_a0, -1);
    return;
  }
  FILE *handle = fopen((const char *)name_str.data(), mode_str);
  if (!handle) {
    rv_set_reg(rv, rv_reg_a0, -1);
    return;
  }
  // find a free file descriptor
  const int fd = find_free_fd(s);
  // insert into the file descriptor map
  s->fd_map[fd] = handle;
  // return the file descriptor
  rv_set_reg(rv, rv_reg_a0, fd);
}

void syscall_handler(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // get the syscall number
  riscv_word_t syscall = rv_get_reg(rv, rv_reg_a7);
  // dispatch call type
  switch (syscall) {
  case SYS_close: 
    syscall_close(rv);
    break;
  case SYS_lseek:
    syscall_lseek(rv);
    break;
  case SYS_read:
    syscall_read(rv);
    break;
  case SYS_write:
    syscall_write(rv);
    break;
  case SYS_fstat:
    syscall_fstat(rv);
    break;
  case SYS_brk:
    syscall_brk(rv);
    break;
  case SYS_exit:
    syscall_exit(rv);
    break;
  case SYS_gettimeofday:
    syscall_gettimeofday(rv);
    break;
  case SYS_open:
    syscall_open(rv);
    break;
#if RISCV_VM_USE_SDL
  case 0xbeef:
    syscall_draw_frame(rv);
    break;
  case 0xbabe:
    syscall_draw_frame_pal(rv);
    break;
#endif
  default:
    fprintf(stderr, "unknown syscall %d\n", int(syscall));
    rv_halt(rv);
    break;
  }
}
