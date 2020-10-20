#include "memory.h"

#ifdef _MSC_VER

#include <Windows.h>
#undef min
#undef max

bool memory_t::init() {

  // note: we could add an exception handler here to only commit pages
  //       if they cause a page fault.  eventually we can use this to
  //       pass page fault exceptions back to the guest program.

  base = (uint8_t*)VirtualAlloc(
    nullptr, 0x100000000llu, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  return base != nullptr;
}

#endif
