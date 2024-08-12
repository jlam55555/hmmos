#include "crt.h"

namespace crt {

namespace detail {

__attribute__((section(".init_array"))) funcptr __start_init_array[0];
__attribute__((section(".fini_array"))) funcptr __start_fini_array[0];

} // namespace detail

/// Additional runtime support functions go here. These don't have to
/// be linked first like the __start_* declarations above, but we put
/// them here so we don't need extra object files and build rules for
/// CRT support.

void run_global_ctors() {
  for (detail::funcptr *ctor = detail::__start_init_array;
       ctor < detail::__stop_init_array; ++ctor) {
    (*ctor)();
  }
}

extern "C" void __cxa_finalize(void *f);
void run_global_dtors() {
  // It's not clear to me whether object destructors (which are
  // inserted using __cxa_atexit()) should be called before global
  // destructors or not. For now I arbitrarily call object destructors
  // first.
  __cxa_finalize(nullptr);

  for (detail::funcptr *dtor = detail::__start_fini_array;
       dtor < detail::__stop_fini_array; ++dtor) {
    (*dtor)();
  }
}

extern "C" void __cxa_pure_virtual() {}

extern "C" void *__dso_handle = 0;

namespace {

// This is an arbitrary limit and can be bumped as necessary.
constexpr unsigned atexit_max_funcs = 128;
unsigned atexit_func_count = 0;

struct AtExitFuncEntry {
  void (*dtor)(void *);
  void *obj_ptr;
  void *dso_handle;
} __attribute__((packed)) atexit_funcs[atexit_max_funcs];

} // namespace

extern "C" int __cxa_atexit(void (*f)(void *), void *obj_ptr, void *dso) {
  if (atexit_func_count >= atexit_max_funcs) {
    return -1;
  }

  auto &entry = atexit_funcs[atexit_func_count++];
  entry.dtor = f;
  entry.obj_ptr = obj_ptr;
  entry.dso_handle = dso;
  return 0;
}

extern "C" void __cxa_finalize(void *f) {
  unsigned i = atexit_func_count;
  while (i--) {
    auto &entry = atexit_funcs[i];
    // According to the Itanium C++ ABI, if __cxa_finalize is called
    // without a function pointer, then it means that we should
    // destroy everything. And we should be careful not to call
    // destructors more than once (unless they were inserted more than
    // once).
    if ((f == nullptr || entry.dtor == f) && entry.dtor) {
      (*entry.dtor)(entry.obj_ptr);
      entry.dtor = nullptr;
    }
  }
  return;
}

} // namespace crt
