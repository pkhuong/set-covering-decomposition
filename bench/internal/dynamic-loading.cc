#include "bench/internal/dynamic-loading.h"

extern "C" {
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
}  // extern "C"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "absl/base/casts.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40
#endif

namespace bench {
namespace internal {
namespace {
std::pair<uintptr_t, uintptr_t> FindEnclosingExecutableRange(
    uintptr_t address) {
  std::ifstream maps("/proc/self/maps");

  std::string line;
  while (maps.good()) {
    std::getline(maps, line);
    unsigned long long begin;
    unsigned long long end;
    if (std::sscanf(line.c_str(), "%llx-%llx r-xp ", &begin, &end) != 2) {
      return {0, 0};
    }

    if (begin <= address && address < end) {
      return {begin, end};
    }
  }

  return {0, 0};
}

void RemapWithHugePages(std::pair<uintptr_t, uintptr_t> range) {
  static constexpr uintptr_t kAlignment = 1UL << 21;
  uintptr_t begin = range.first;
  uintptr_t end = range.second;

  // Round the beginning of the region up to 2MB: huge page must be aligned.
  if (begin % kAlignment != 0) {
    begin = kAlignment * (1 + begin / kAlignment);
  }

  // Round the end of the region down to 2MB: we can't allocate
  // partial huge pages.
  end &= -kAlignment;

  if (end <= begin) {
    std::cerr << "Unable to remap [" << absl::bit_cast<void *>(begin) << ", "
              << absl::bit_cast<void *>(end) << "): aligned range is empty."
              << std::endl;
    return;
  }

  const size_t size = end - begin;
  auto buf = absl::make_unique<char[]>(size);

  std::memcpy(static_cast<void *>(buf.get()),
              absl::bit_cast<const void *>(begin), size);

  // We can't mremap huge anonymous mappings. Overwrite with a fresh
  // mapping and memcpy, like savages.
  {
    // But first, make sure we can map that much in: MAP_FIXED isn't
    // atomic and *will* fail after erasing the old mapping.
    void *temp_mapping = mmap(NULL, size, PROT_NONE,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (temp_mapping == MAP_FAILED) {
      std::perror("Failed to reserve temporary huge page region");
      return;
    }

    munmap(temp_mapping, size);
  }

  void *new_mapping =
      mmap(absl::bit_cast<void *>(begin), size, PROT_READ | PROT_WRITE,
           MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (new_mapping == MAP_FAILED) {
    std::perror("Failed to map in huge page region");
    return;
  }

  std::memcpy(new_mapping, static_cast<const void *>(buf.get()), size);
  mprotect(absl::bit_cast<void *>(begin), size, PROT_EXEC | PROT_READ);
}

void TryToRemapTextMapping(absl::string_view file, uintptr_t address) {
  const std::pair<uintptr_t, uintptr_t> range =
      FindEnclosingExecutableRange(address);
  if (range.second > range.first) {
    std::cerr << "Attempting to remap " << file << " with huge pages."
              << std::endl;
    RemapWithHugePages(range);
  } else {
    std::cerr << "Unable to find range enclosing "
              << absl::bit_cast<void *>(address) << " in /proc/self/maps."
              << std::endl;
  }
}
}

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

std::pair<void *, DLCloser> DLMOpenOrDie(const std::string &file,
                                         const std::string &symbol,
                                         const OpenOptions &options) {
  const char *name;
  void *handle;
#ifdef DLMOPEN_AVAILABLE
  if (options.dlmopen) {
    name = "dlmopen";
    handle = dlmopen(LM_ID_NEWLM, file.c_str(), RTLD_LOCAL | RTLD_NOW);
  } else
#endif
  {
    name = "dlopen";
    handle = dlopen(file.c_str(), RTLD_LOCAL | RTLD_NOW);
  }

  if (handle == nullptr) {
    std::cerr << name << "(" << file << ") failed: " << dlerror() << std::endl;
    abort();
  }

  void *fn_ptr = dlsym(handle, symbol.c_str());
  if (fn_ptr == nullptr) {
    std::cerr << "dlsym(" << symbol << ") failed in " << file << ": "
              << dlerror() << std::endl;
    abort();
  }

  if (options.remap) {
    TryToRemapTextMapping(file, absl::bit_cast<uintptr_t>(fn_ptr));
  }
  return std::make_pair(fn_ptr, DLCloser(handle));
}

std::pair<void *, DLCloser> DLOpenOrDie(const std::string &file,
                                        const std::string &symbol,
                                        const OpenOptions &options) {
  void *handle = dlopen(file.c_str(), RTLD_LOCAL | RTLD_NOW);
  if (handle == nullptr) {
    std::cerr << "dlopen(" << file << ") failed: " << dlerror() << std::endl;
    abort();
  }

  void *fn_ptr = dlsym(handle, symbol.c_str());
  if (fn_ptr == nullptr) {
    std::cerr << "dlsym(" << symbol << ") failed in " << file << ": "
              << dlerror() << std::endl;
    abort();
  }

  if (options.remap) {
    TryToRemapTextMapping(file, absl::bit_cast<uintptr_t>(fn_ptr));
  }
  return std::make_pair(fn_ptr, DLCloser(handle));
}
}  // namespace internal
}  // namespace bench
