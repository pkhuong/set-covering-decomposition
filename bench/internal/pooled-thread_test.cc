#include "bench/internal/pooled-thread.h"

#include "absl/synchronization/notification.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace bench {
namespace internal {
namespace {
TEST(PooledThread, Cancelled) {
  EXPECT_FALSE(PooledThread::Cancelled());

  absl::Notification running;
  absl::Notification ready;
  bool cancelled = true;
  PooledThread worker([&running, &ready, &cancelled] {
    cancelled = PooledThread::Cancelled();

    running.Notify();

    ready.WaitForNotification();
    cancelled = PooledThread::Cancelled();
  });

  running.WaitForNotification();
  EXPECT_FALSE(cancelled);

  worker.Cancel();
  ready.Notify();
  worker.Join();
  EXPECT_TRUE(cancelled);
}

TEST(PooledThread, Moved) {
  PooledThread worker([] {
    while (!PooledThread::Cancelled()) {
    }
  });

  PooledThread other(std::move(worker));

  other.Cancel();
  other.Join();
}

TEST(PooledThread, Destroyed) {
  PooledThread worker([] {
    while (!PooledThread::Cancelled()) {
    }
  });
}
}  // namespace
}  // namespace internal
}  // namespace bench
