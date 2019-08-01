#include "bench/internal/pooled-thread.h"

#include <atomic>
#include <cstdlib>
#include <thread>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace bench {
namespace internal {
// This private implementation is backed by a persistent, detached,
// std::thread.
class PooledThread::Impl {
 public:
  class FreeList;

  Impl();
  ~Impl() = delete;

  // Returns whether the calling thread is a `PooledThread` that has
  // been cancelled since the last call to `Start()`.
  static bool Cancelled();

  // Transitions from kIdle -> kWorkQueued, to let the worker thread
  // know it should execute `work`.
  void Start(std::function<void()> work);

  // Marks the current work unit as cancelled;
  void Cancel();

  // Transitions from kDone -> kIdle.
  void Join();

 private:
  enum class State {
    kIdle = 0,
    kWorkQueued = 1,
    kExecuting = 2,
    kDone = 3,
  };

  static thread_local Impl* self;

  void WorkLoop();

  // Writes are serialised with `mu_` for simplicity, and do not
  // happen on the worker thread.
  std::atomic<bool> cancelled_{false};

  absl::Mutex mu_;
  State state_{State::kIdle};
  std::function<void()> work_;
};

thread_local PooledThread::Impl* PooledThread::Impl::self = nullptr;

class PooledThread::Impl::FreeList {
 public:
  // Returns a PooledThread::Impl, newly created or recycled from the
  // global free list.
  static ImplPtr GetBackingThread() { return GetSingleton()->Get(); }

  // Puts `impl` back on the global free list.
  static void ReleaseBackingThread(ImplPtr impl) {
    return GetSingleton()->Release(std::move(impl));
  }

 private:
  // Grabs a PooledThread::Impl from the freelist, or creates a fresh
  // one on demand.
  ImplPtr Get();

  // Puts `impl` back on the free list.
  void Release(ImplPtr impl);

  // Returns the global FreeList of PooledThread::Impl.
  static FreeList* GetSingleton();

  absl::Mutex mu_;
  std::vector<ImplPtr> available_ GUARDED_BY(mu_);
};

PooledThread::Impl::Impl() {
  // Kick off a worker thread that will wait for something to do in
  // `work_`.
  std::thread worker([this] { WorkLoop(); });
  worker.detach();
}

bool PooledThread::Impl::Cancelled() {
  Impl* impl = self;
  return impl != nullptr && impl->cancelled_.load();
}

void PooledThread::Impl::Start(std::function<void()> work) {
  absl::MutexLock ml(&mu_);
  if (state_ != State::kIdle) {
    abort();
  }

  using std::swap;
  swap(work_, work);
  state_ = State::kWorkQueued;
  cancelled_.store(false);
}

void PooledThread::Impl::Cancel() {
  absl::MutexLock ml(&mu_);
  cancelled_.store(true);
}

void PooledThread::Impl::Join() {
  absl::MutexLock ml(&mu_);
  mu_.Await(absl::Condition(
      +[](State* state) {
        return *state == State::kIdle || *state == State::kDone;
      },
      &state_));
  state_ = State::kIdle;
}

void PooledThread::Impl::WorkLoop() {
  self = this;
  for (;;) {
    {
      absl::MutexLock ml(&mu_);
      mu_.Await(absl::Condition(
          +[](State* state) { return *state == State::kWorkQueued; }, &state_));
      state_ = State::kExecuting;
    }

    work_();
    {
      absl::MutexLock ml(&mu_);
      state_ = State::kDone;
    }
  }
}

PooledThread::ImplPtr PooledThread::Impl::FreeList::Get() {
  {
    absl::MutexLock ml(&mu_);
    if (!available_.empty()) {
      auto ret = std::move(available_.back());
      available_.pop_back();
      return ret;
    }
  }

  return ImplPtr(new Impl);
}

void PooledThread::Impl::FreeList::Release(PooledThread::ImplPtr impl) {
  if (impl == nullptr) {
    return;
  }

  absl::MutexLock ml(&mu_);
  available_.push_back(std::move(impl));
}

PooledThread::Impl::FreeList* PooledThread::Impl::FreeList::GetSingleton() {
  static auto* const singleton = new FreeList;

  return singleton;
}

PooledThread::PooledThread(std::function<void()> work)
    : impl_(Impl::FreeList::GetBackingThread()) {
  impl_->Start(std::move(work));
}

PooledThread::~PooledThread() {
  if (impl_ != nullptr) {
    impl_->Cancel();
    impl_->Join();
    Impl::FreeList::ReleaseBackingThread(std::move(impl_));
  }
}

bool PooledThread::Cancelled() { return Impl::Cancelled(); }

void PooledThread::Cancel() { impl_->Cancel(); }

void PooledThread::Join() { impl_->Join(); }
}  // namespace internal
}  // namespace bench
