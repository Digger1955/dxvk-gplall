#pragma once

#include <cmath>
#include <cstddef>
#include <type_traits>

#if defined(__SSE2__)
  #include <emmintrin.h>
#endif

namespace dxvk {

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
    static_assert(std::is_integral_v<T> && std::is_integral_v<U>,
                  "align(): T and U must be integral types");

    return static_cast<T>(((what + static_cast<T>(to) - 1) / static_cast<T>(to)) * static_cast<T>(to));
  }


  template<typename T, typename U = T>
  constexpr T alignDown(T what, U to) {
    static_assert(std::is_integral_v<T> && std::is_integral_v<U>,
                  "alignDown(): T and U must be integral types");
    return static_cast<T>((what / static_cast<T>(to)) * static_cast<T>(to));
  }

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
    static_assert(std::is_integral_v<T>, "divCeil(): T must be an integral type");
    return (dividend + divisor - 1) / divisor;
  }
  
}
