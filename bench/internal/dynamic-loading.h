#ifndef BENCH_INTERNAL_DYNAMIC_LOADING_H
#define BENCH_INTERNAL_DYNAMIC_LOADING_H
#include <string>
#include <utility>

#include "absl/base/macros.h"

namespace bench {
namespace internal {
// A `DLCloser` is an RAII wrapper around `dlopen(3)/dlmopen(3)` and
// `dlclose(3)`.  If the `handle` passed in is non-null, it will be
// `dlclose`d when the `DLCloser` is destroyed.
//
// If `dlclose` fails, the destructor will abort noisily.
class DLCloser {
 public:
  explicit DLCloser(void *handle) : handle_(handle) {}

  ~DLCloser();

  // Not copyable or assignable, but movable.
  DLCloser(const DLCloser &) = delete;
  DLCloser &operator=(const DLCloser &) = delete;
  DLCloser &operator=(DLCloser &&) = delete;

  DLCloser(DLCloser &&other) : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

 private:
  void *handle_;
};

struct OpenOptions {
  bool remap{true};
  bool dlmopen{false};

  // If true, the dynamic loading code will attempt to remap the code
  // to test with huge pages.
  OpenOptions &SetRemap(bool value) {
    remap = value;
    return *this;
  }

  // If true, the dynamic loading code will hermetically load code
  // with dlmopen.  This provides more isolation (we don't inherit the
  // timing harness's libc or stdlibc++), but crash prone because libc
  // becomes *really* confused when it's loaded multiple times.
  OpenOptions &SetDLMOpen(bool value) {
    dlmopen = value;
    return *this;
  }
};

// Attempts to `dlmopen(3)` `file`, and looks for `symbol` (which must
// be correctly name-mangled) in that shared object.
//
// If `remap` is true, attempt to remap the .text section with huge
// pages.
//
// Dies noisily on failure.
std::pair<void *, DLCloser> DLMOpenOrDie(
    const std::string &file, const std::string &symbol,
    const OpenOptions &options = OpenOptions());

// Attempts to `dlopen(3)` `file`, and looks for `symbol` (which must
// be correctly name-mangled) in that shared object.
//
// If `remap` is true, attempt to remap the .text section with huge
// pages.
//
// Dies noisily on failure.
//
// This function should only be called if the `symbol` must re-use the
// calling environment's C++ symbols.  Ideally, the interfaces should
// be re-worked to remove that requirement.
std::pair<void *, DLCloser> DLOpenOrDie(
    const std::string &file, const std::string &symbol,
    const OpenOptions &options = OpenOptions());

// Hide the deprecated function in another template that's
// instantiated with a non-specialised template argument to make the
// deprecation notice fire lazily, on instantiation.
template <bool kIsProbablyABISafe>
struct BadABIIndependenceNotice;

template <>
struct BadABIIndependenceNotice<true> {
  // ABI safe is fine, nothing to complain about.
  static void Notice() {}
};

template <>
struct BadABIIndependenceNotice<false> {
  static ABSL_DEPRECATED(
      "Timing functions should have ABI-safe argument and return types to "
      "ensure independence from the benchmarking harness's C and C++ runtime "
      "systems.") void Notice() {}
};

template <bool kIsProbablyABISafe,
          typename =
              decltype(&BadABIIndependenceNotice<kIsProbablyABISafe>::Notice)>
struct OpenOrDie;

template <>
struct OpenOrDie<true, void (*)()> {
  std::pair<void *, DLCloser> operator()(
      const std::string &file, const std::string &symbol,
      const OpenOptions &options = OpenOptions()) {
    return DLMOpenOrDie(file, symbol, options);
  }
};

template <>
struct OpenOrDie<false, void (*)()> {
  std::pair<void *, DLCloser> operator()(
      const std::string &file, const std::string &symbol,
      const OpenOptions &options = OpenOptions()) {
    return DLOpenOrDie(file, symbol, options);
  }
};
}  // namespace internal
}  // namespace bench
#endif /* !BENCH_INTERNAL_DYNAMIC_LOADING_H */
