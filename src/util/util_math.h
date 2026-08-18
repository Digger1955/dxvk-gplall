#pragma once

#include <cmath>

#if defined(__SSE2__)
  #include <emmintrin.h>
#endif

namespace dxvk {

  /*
  IMPORTANT: 
  1. Use unaligned loads/stores (_mm_loadu_ps/_mm_storeu_ps) for safety,
  because Matrix4/Vector4 may be placed in memory without guaranteed
  16-byte alignment in all places (mapped buffers, packed structs, etc).
  2. DO NOT USE alignas(16), because with a Windows target platform, 
  GCC will just not align rsp at all and silently generate code that can explode at any time.
  Info: https://github.com/doitsujin/dxvk/pull/4448
  */

  constexpr size_t CACHE_LINE_SIZE = 64;

  constexpr double pi = 3.14159265359;

  template<typename T>
  constexpr T clamp(T n, T lo, T hi) {
    if (n < lo) return lo;
    if (n > hi) return hi;
    return n;
  }

  template<typename T, typename U = T>
  constexpr T align(T what, U to) {
    return (what + to - 1) & ~(to - 1);
  }

  template<typename T, typename U = T>
  constexpr T alignDown(T what, U to) {
    return (what / to) * to;
  }

  // Equivalent of std::clamp for use with floating point numbers
  // Handles (-){INFINITY,NAN} cases.
  // Will return min in cases of NAN, etc.
  [[nodiscard]] inline float fclamp(float value, float min, float max) {
  #if defined(__SSE2__)

    __m128 v  = _mm_set_ss(value);
    __m128 mn = _mm_set_ss(min);
    __m128 mx = _mm_set_ss(max);

    v = _mm_max_ss(v, mn);
    v = _mm_min_ss(v, mx);

    float result;
    _mm_store_ss(&result, v);
    return result;
  #else

    return std::fmin(std::fmax(value, min), max);
  #endif
  }

  template<typename T>
  inline T divCeil(T dividend, T divisor) {
    return (dividend + divisor - 1) / divisor;
  }
  
}