#ifndef PRNG_H
#define PRNG_H
#include <array>
#include <cstdint>
#include <limits>

class xs256 {
 public:
  using result_type = uint64_t;

  // Constructs an independent stream.
  xs256();

  // Copyable.
  xs256(const xs256&) = default;
  xs256& operator=(const xs256&) = default;

  uint64_t Uniform(uint64_t limit) {
    unsigned __int128 tmp = limit;

    tmp *= (*this)();
    return tmp >> 64;
  }

  // The xoroshiro256+ generator has some bias in the low order bits,
  // but we only use its high-order bits (as if generating floats).
  uint64_t operator()() {
    const uint64_t result_plus = state_[0] + state_[3];

    Advance(&state_);
    return result_plus;
  }

  double entropy() const { return 0.0; }
  static uint64_t min() { return 0; }
  static uint64_t max() { return std::numeric_limits<uint64_t>::max(); }

  static void Advance(std::array<uint64_t, 4>* state) {
    const uint64_t t = (*state)[1] << 17;

    (*state)[2] ^= (*state)[0];
    (*state)[3] ^= (*state)[1];
    (*state)[1] ^= (*state)[2];
    (*state)[0] ^= (*state)[3];

    (*state)[2] ^= t;

    (*state)[3] = rotl((*state)[3], 45);
  }

 private:
  using State = std::array<uint64_t, 4>;

  static uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
  }

  State state_;
};
#endif /*!PRNG_H */
