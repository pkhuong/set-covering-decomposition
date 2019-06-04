#include "big-vec.h"

#include <sys/mman.h>

#include <cstdio>
#include <iostream>

namespace {
constexpr size_t kOneGb = 1024 * 1024 * 1024;
constexpr size_t kTwoMb = 2 * 1024 * 1024;

thread_local BigVecArena *local_arena = nullptr;

void Unmap(void *ptr, size_t length) {
  int r = munmap(ptr, length);
  (void)r;
  assert(r == 0);
}
}  // namespace

BigVecArena &BigVecArena::default_instance() {
  BigVecArena *local = local_arena;
  if (local != nullptr) {
    return *local;
  }

  static auto &arena = *new BigVecArena;
  return arena;
}

BigVecArenaContext::BigVecArenaContext() : previous(local_arena) {
  owned.emplace();
  local_arena = &owned.value();
}

BigVecArenaContext::BigVecArenaContext(BigVecArena *arena)
    : previous(local_arena) {
  local_arena = arena;
}

BigVecArenaContext::~BigVecArenaContext() { local_arena = previous; }

BigVecArena::BigVecArena() = default;

BigVecArena::~BigVecArena() {
  absl::MutexLock ml(&mu_);
  for (auto &entry : cache_) {
    for (void *ptr : entry.second) {
      Unmap(ptr, entry.first);
    }
  }
}

void BigVecArena::Recycle(void *data, size_t byte_size) {
  if (byte_size == 0) {
    return;
  }

  absl::MutexLock ml(&mu_);
  cache_[byte_size].push_back(data);
}

void *BigVecArena::AcquireRoundedBytes(size_t exact_size, int flags) {
  void *r = mmap(nullptr, exact_size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | flags, -1, 0);
  if (r == MAP_FAILED) {
    perror("mmap");
    return nullptr;
  }

  return r;
}

#if defined(MAP_HUGE_SHIFT) && defined(MAP_HUGETLB)
#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

#ifndef MAP_HUGE_1GB
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)
#endif

#else
#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB 0
#endif

#ifndef MAP_HUGE_1GB
#define MAP_HUGE_1GB 0
#endif

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0
#endif
#endif

std::pair<void *, size_t> BigVecArena::AcquireBytes(size_t min_size) {
  const auto round = [min_size](size_t page) {
    return page * ((min_size + page - 1) / page);
  };

  assert(min_size > 0);
  if (min_size == 0) {
    min_size = 1;
  }

  // First, check the cache.
  {
    absl::MutexLock ml(&mu_);

    for (size_t exact_size : {round(kOneGb), round(kTwoMb), round(4096)}) {
      auto &list = cache_[exact_size];
      if (!list.empty()) {
        void *ret = list.back();
        list.pop_back();
        return std::make_pair(ret, exact_size);
      }
    }
  }

  std::clog << "Looking for a vector of " << min_size << " bytes\n";
  if (MAP_HUGE_1GB != 0 && min_size >= kOneGb) {
    size_t exact_size = round(kOneGb);
    void *ret = AcquireRoundedBytes(exact_size, MAP_HUGETLB | MAP_HUGE_1GB);
    if (ret != nullptr) {
      std::clog << "Acquired " << exact_size / kOneGb << "GB.\n";
      return std::make_pair(ret, exact_size);
    }
  }

  if (MAP_HUGE_2MB != 0 && min_size >= kTwoMb) {
    size_t exact_size = round(kTwoMb);
    void *ret = AcquireRoundedBytes(exact_size, MAP_HUGETLB | MAP_HUGE_2MB);
    if (ret != nullptr) {
      std::clog << "Acquired " << 2 * exact_size / kTwoMb << "MB.\n";
      return std::make_pair(ret, exact_size);
    }
  }

  size_t exact_size = round(4096);
  void *ret = nullptr;
  if (MAP_HUGETLB != 0) {
    ret = AcquireRoundedBytes(exact_size, MAP_HUGETLB);
  }
  if (ret == nullptr) {
    ret = AcquireRoundedBytes(exact_size, 0);
  }

  std::clog << "Acquired " << exact_size / 1024 << "KB: " << ret << "\n";
  assert(ret != nullptr);
  return std::make_pair(ret, exact_size);
}
