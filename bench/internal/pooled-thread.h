#ifndef BENCH_INTERNAL_POOLED_THREAD_H
#define BENCH_INTERNAL_POOLED_THREAD_H
#include <functional>
#include <memory>
#include <utility>

namespace bench {
namespace internal {
// A PooledThread looks like a std::thread, but does not ever release
// or join its backing pthread; the OS thread is always recycled back
// to a global pool on destruction, and detached so as not to block
// shutdown.
//
// We must avoid pthread destruction because cleaning up thread-local
// storage can be crashy in the presence of multiple dlmopen-ed C and
// C++ standard libraries.
class PooledThread {
 public:
  // Kicks off `work` in a worker thread.
  explicit PooledThread(std::function<void()> work);

  // If `this` PooledThread is still valid, `Cancel()`s and `Join()`
  // is before recycling the underlying thread.
  ~PooledThread();

  // Move-only.  The moved-from thread is now invalid.
  PooledThread(PooledThread&&) = default;

  PooledThread(const PooledThread&) = delete;
  PooledThread& operator=(const PooledThread&) = delete;
  PooledThread& operator=(PooledThread&&) = delete;

  // Returns whether the calling thread is a `PooledThread` that has
  // been cancelled.
  static bool Cancelled();

  // Marks the thread as `Cancelled()`.  Cancellation does nothing in
  // itself, it only sets an internal flag that user code can check
  // via `PooledThread::Cancelled` to exit gracefully.
  void Cancel();

  // Waits until the thread is done working.
  void Join();

  friend void swap(PooledThread& x, PooledThread& y) {
    using std::swap;
    swap(x.impl_, y.impl_);
  }

 private:
  class FreeList;
  class Impl;

  struct NoDelete {
    // We never delete Impl objects.
    void operator()(Impl*) {}
  };
  using ImplPtr = std::unique_ptr<Impl, NoDelete>;

  ImplPtr impl_;
};
}  // namespace internal
}  // namespace bench
#endif /* !BENCH_INTERNAL_POOLED_THREAD_H */
