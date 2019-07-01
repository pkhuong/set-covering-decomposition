#ifndef BENCH_CONSTRUCTABLE_ARRAY_H
#define BENCH_CONSTRUCTABLE_ARRAY_H
#include <array>
#include <cstddef>
#include <type_traits>

namespace bench {
namespace internal {
// A `ConstructableArray` is an `std::array` with a trivial constructor.
// All the storage is left uninitialized until `EmplaceAt` is called exactly
// once for each index 0 <= i < n.
//
// The destructor is also trivial: `Destroy()` must be called
// explicitly, exactly once, after all the elements have been emplaced
// once.
//
// This class is hard to use correctly, and exists solely for
// implementation code in `bench/` that is sensitive to measurement
// noise.
template <typename T, size_t n>
class ConstructableArray {
  using Element = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

  using Alloc = std::allocator<T>;

  using AllocTraits = std::allocator_traits<Alloc>;

 public:
  ConstructableArray() = default;
  ~ConstructableArray() = default;

  // Delete copy and assignment: we shouldn't need any of that.
  ConstructableArray(const ConstructableArray&) = delete;
  ConstructableArray& operator=(const ConstructableArray&) = delete;

  T& operator[](size_t i) { return data()[i]; }
  const T& operator[](size_t i) const { return data()[i]; }

  T& front() { return data()[0]; }
  T& back() { return data()[(size() - 1)]; }

  template <typename... Args>
  void EmplaceAt(size_t i, Args&&... args) {
    Alloc alloc;

    AllocTraits::construct(alloc, data() + i, std::forward<Args>(args)...);
    // Make sure the result is fully populated before returning.  This
    // prevents the compiler from moving the construction code inside
    // the section we're trying to time.
    asm volatile("" ::"X"(data() + i) : "memory");
  }

  void Destroy() {
    Alloc alloc;

    for (size_t i = 0; i < size(); ++i) {
      AllocTraits::destroy(alloc, data() + i);
    }
  }

  T* data() { return reinterpret_cast<T*>(backing_.data()); }

  const T* data() const { return reinterpret_cast<const T*>(backing_.data()); }

  size_t size() const { return n; }

 private:
  // This is a C array of opaque bytes that have the correct size and
  // alignment to construct `T`s in.
  std::array<Element, n> backing_;
};
}  // namespace internal
}  // namespace bench
#endif /*!BENCH_CONSTRUCTABLE_ARRAY_H */
