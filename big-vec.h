#ifndef BIG_VEC_H
#define BIG_VEC_H
#include <cstddef>
#include <type_traits>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"

class BigVecArena;

template <typename T>
class BigVec {
  static_assert(std::is_trivial<T>::value,
                "BigVec can only hold trivial types (e.g., POD).");

 public:
  using value_type = T;
  using iterator = T*;
  using const_iterator = const T*;

  BigVec() : BigVec(nullptr, 0, 0, nullptr) {}

  // Test-only convenience constructor.
  BigVec(std::initializer_list<T> init);

  // Movable.
  BigVec(const BigVec<T>& other) = delete;
  BigVec(BigVec<T>&& other) : BigVec() { Swap(&other); }

  BigVec<T>& operator=(const BigVec<T>& other) = delete;
  BigVec<T>& operator=(BigVec<T>&& other) {
    Swap(&other);
    return *this;
  }

  ~BigVec();

  // Wrap this in something useful like an `absl::Span` to use the
  // storage.
  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }
  size_t size() const noexcept { return size_; };

  T& operator[](size_t i) noexcept { return data_[i]; }
  const T& operator[](size_t i) const noexcept { return data_[i]; }

  T* begin() noexcept { return data(); }
  T* end() noexcept { return data() + size(); }
  const T* begin() const noexcept { return data(); }
  const T* end() const noexcept { return data() + size(); }

  const T* cbegin() const noexcept { return begin(); }
  const T* cend() const noexcept { return end(); }

  void clear();

  bool operator==(const BigVec<T>& other) const;
  bool operator!=(const BigVec<T>& other) const { return !(*this == other); }

  void Swap(BigVec<T>* other) {
    using std::swap;
    swap(data_, other->data_);
    swap(byte_size_, other->byte_size_);
    swap(size_, other->size_);
    swap(parent_, other->parent_);
  }

 private:
  friend class BigVecArena;

  BigVec(T* data, size_t byte_size, size_t size, BigVecArena* parent)
      : data_(data), byte_size_(byte_size), size_(size), parent_(parent) {}
  BigVec(T* data, size_t byte_size, size_t size, BigVecArena* parent,
         const T& init)
      : data_(data), byte_size_(byte_size), size_(size), parent_(parent) {
    // XXX: we must use placement new to avoid UB.
    std::fill_n(data_, size_, init);
  }

  T* data_;
  size_t byte_size_;
  size_t size_;
  BigVecArena* parent_;
};

// This class is thread-safe.
class BigVecArena {
 public:
  static BigVecArena& default_instance();

  BigVecArena();
  ~BigVecArena();

  // Not assignable or movable.
  BigVecArena(BigVecArena&) = delete;
  BigVecArena& operator=(BigVecArena&) = delete;

  template <typename T>
  BigVec<T> Create(size_t count, const T& init = T()) {
    void* data;
    size_t byte_size;
    if (count == 0) {
      data = EmptyAlloc<T>();
      byte_size = 0;
    } else {
      std::tie(data, byte_size) = AcquireBytes(sizeof(T) * count);
    }
    return BigVec<T>(static_cast<T*>(data), byte_size, count, this, init);
  }

  template <typename T>
  BigVec<T> CreateUninit(size_t count) {
    void* data;
    size_t byte_size;
    if (count == 0) {
      data = EmptyAlloc<T>();
      byte_size = 0;
    } else {
      std::tie(data, byte_size) = AcquireBytes(sizeof(T) * count);
    }
    return BigVec<T>(static_cast<T*>(data), byte_size, count, this);
  }

  void Recycle(void* data, size_t byte_size);

 private:
  template <typename T>
  static void* EmptyAlloc();

  // Attempts to return an allocation for `exact_size` bytes, passing
  // `flags` to mmap if the cache is empty.
  void* AcquireRoundedBytes(size_t exact_size, int flags);

  // Returns an allocation for at least `min_size` bytes.
  std::pair<void*, size_t> AcquireBytes(size_t min_size);

  absl::Mutex mu_;
  absl::flat_hash_map<size_t, std::vector<void*>> cache_ GUARDED_BY(mu_);
};

// RAII class to override BigVecArena::global() within a dynamic
// scope.
class BigVecArenaContext {
 public:
  explicit BigVecArenaContext();
  explicit BigVecArenaContext(BigVecArena* arena);
  ~BigVecArenaContext();

  BigVecArenaContext(BigVecArenaContext&) = delete;
  BigVecArenaContext& operator=(BigVecArenaContext&) = delete;

 private:
  absl::optional<BigVecArena> owned;
  BigVecArena* previous;
};

template <typename T>
BigVec<T>::~BigVec<T>() {
  if (data_ == nullptr) {
    return;
  }

  parent_->Recycle(data_, byte_size_);
  data_ = nullptr;
  byte_size_ = 0;
  size_ = 0;
  parent_ = nullptr;
}

template <typename T>
BigVec<T>::BigVec(std::initializer_list<T> init) : BigVec() {
  *this = BigVecArena::default_instance().Create<T>(init.size());
  size_t i = 0;
  for (const T& x : init) {
    data_[i++] = x;
  }
}

template <typename T>
void BigVec<T>::clear() {
  *this = parent_->Create<T>(0);
}

template <typename T>
bool BigVec<T>::operator==(const BigVec<T>& other) const {
  if (size() != other.size()) {
    return false;
  }

  for (size_t i = 0; i < size(); ++i) {
    if (data()[i] != other.data()[i]) {
      return false;
    }
  }

  return true;
}

template <typename T>
void* BigVecArena::EmptyAlloc() {
  static T* const empty = new T[0];
  return static_cast<void*>(empty);
}
#endif /* !BIG_VEC_H */
