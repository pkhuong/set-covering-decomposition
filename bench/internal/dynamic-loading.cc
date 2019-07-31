#include "bench/internal/dynamic-loading.h"

extern "C" {
#include <dlfcn.h>
}  // extern "C"

#include <cstdlib>
#include <iostream>

namespace bench {
namespace internal {
DLCloser::~DLCloser() {
  if (handle_ == nullptr) {
    return;
  }

  if (dlclose(handle_) != 0) {
    std::cerr << "dlclose(" << handle_ << ") failed: " << dlerror()
              << std::endl;
    abort();
  }

  handle_ = nullptr;
}

std::pair<void *, DLCloser> DLMOpenOrDie(absl::string_view file,
                                         absl::string_view symbol) {
#ifdef DLMOPEN_AVAILABLE
  static const char name[] = "dlmopen";
  void *handle = dlmopen(LM_ID_NEWLM, file.data(), RTLD_LOCAL | RTLD_NOW);
#else
  static const char name[] = "dlopen";
  void *handle = dlopen(file.data(), RTLD_LOCAL | RTLD_NOW);
#endif
  if (handle == nullptr) {
    std::cerr << name << "(" << file << ") failed: " << dlerror() << std::endl;
    abort();
  }

  void *fn_ptr = dlsym(handle, symbol.data());
  if (fn_ptr == nullptr) {
    std::cerr << "dlsym(" << symbol << ") failed in " << file << ": "
              << dlerror() << std::endl;
    abort();
  }

  return std::make_pair(fn_ptr, DLCloser(handle));
}

std::pair<void *, DLCloser> DLOpenOrDie(absl::string_view file,
                                        absl::string_view symbol) {
  void *handle = dlopen(file.data(), RTLD_LOCAL | RTLD_NOW);
  if (handle == nullptr) {
    std::cerr << "dlopen(" << file << ") failed: " << dlerror() << std::endl;
    abort();
  }

  void *fn_ptr = dlsym(handle, symbol.data());
  if (fn_ptr == nullptr) {
    std::cerr << "dlsym(" << symbol << ") failed in " << file << ": "
              << dlerror() << std::endl;
    abort();
  }

  return std::make_pair(fn_ptr, DLCloser(handle));
}
}  // namespace internal
}  // namespace bench
