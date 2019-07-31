#ifndef BENCH_STABLE_UNIQUE_PTR_H
#define BENCH_STABLE_UNIQUE_PTR_H
#include <memory>
#include <type_traits>

namespace bench {
// A StableUniquePtr is an ABI-stable view of T, along with an opaque
// pair of backing storage pointer and deleter function.
template <typename T>
class StableUniquePtr {
 public:
  StableUniquePtr() : value_(nullptr), backing_(nullptr), deleter_(nullptr) {}

  ~StableUniquePtr() { reset(); }

  // Accept the unique_ptr by rvalue reference instead of by value to
  // make it easier to extract `value` from `backing` in the call, without
  // running afoul of null moved-from unique_ptrs.
  template <typename U>
  StableUniquePtr(T* value, std::unique_ptr<U>&& backing)
      : value_(value),
        backing_(static_cast<void*>(backing.release())),
        deleter_(+[](void* ptr) { delete static_cast<U*>(ptr); }) {}

  StableUniquePtr(const StableUniquePtr&) = delete;

  template <typename U,
            typename = typename std::enable_if<std::is_convertible<U, T>::value,
                                               void>::type>
  StableUniquePtr(StableUniquePtr<U>&& other)
      : value_(other.value_),
        backing_(other.backing_),
        deleter_(other.deleter_) {
    other.value_ = nullptr;
    other.backing_ = nullptr;
    other.deleter_ = nullptr;
  }

  StableUniquePtr& operator=(const StableUniquePtr&) = delete;

  template <typename U,
            typename = typename std::enable_if<std::is_convertible<U, T>::value,
                                               void>::type>
  StableUniquePtr& operator=(StableUniquePtr<U>&& other) {
    reset();
    using std::swap;
    swap(value_, other.value_);
    swap(backing_, other.backing_);
    swap(deleter_, other.deleter_);
  }

  T& operator*() const { return *value_; }
  T* operator->() const noexcept { return value_; }
  T* get() const noexcept { return value_; }

  void swap(StableUniquePtr& other) {
    using std::swap;

    swap(value_, other.value_);
    swap(backing_, other.backing_);
    swap(deleter_, other.deleter_);
  }

  void reset() {
    if (deleter_ != nullptr) {
      deleter_(static_cast<void*>(backing_));
    }

    value_ = nullptr;
    backing_ = nullptr;
    deleter_ = nullptr;
  }

 private:
  template <typename U>
  friend class StableUniquePtr;

  T* value_;
  void* backing_;
  void (*deleter_)(void*);
};

// Accept the unique_ptr by rvalue reference instead of by value to
// make it easier to extract `value` from `backing` in the call, without
// running afoul of null moved-from unique_ptrs.
template <typename T, typename U>
StableUniquePtr<T> MakeStableUniquePtr(T* value, std::unique_ptr<U>&& backing) {
  return StableUniquePtr<T>(value, std::move(backing));
}
}  // namespace bench
#endif /* !BENCH_STABLE_UNIQUE_PTR_H */
