#pragma once

#include <iostream>
#include <cmath>
#include <cstddef>

#if defined(__SSE4_1__) || defined(__SSE2__)
  #include <emmintrin.h>
#endif
#if defined(__SSE4_1__)
  #include <smmintrin.h> // _mm_dp_ps
#endif

#include "util_bit.h"
#include "util_math.h"

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

  // PRIMARY TEMPLATE

  template <typename T>
  struct Vector4Base {
    union {
      T data[4];
      struct { T x, y, z, w; };
      struct { T r, g, b, a; };
    };

    Vector4Base() : x{ }, y{ }, z{ }, w{ } { }
    Vector4Base(T splat) : x(splat), y(splat), z(splat), w(splat) { }
    Vector4Base(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) { }
    Vector4Base(const T xyzw[4]) : x(xyzw[0]), y(xyzw[1]), z(xyzw[2]), w(xyzw[3]) { }

    Vector4Base(const Vector4Base<T>& other) = default;
    Vector4Base& operator=(const Vector4Base<T>& other) = default;

    inline       T& operator[](size_t index)       { return data[index]; }
    inline const T& operator[](size_t index) const { return data[index]; }

    bool operator==(const Vector4Base<T>& other) const {
      for (uint32_t i = 0; i < 4; i++) {
        if (data[i] != other.data[i])
          return false;
      }
      return true;
    }

    bool operator!=(const Vector4Base<T>& other) const {
      return !operator==(other);
    }

    Vector4Base operator-() const { return {-x, -y, -z, -w}; }

    Vector4Base operator+(const Vector4Base<T>& other) const {
      return {x + other.x, y + other.y, z + other.z, w + other.w};
    }

    Vector4Base operator-(const Vector4Base<T>& other) const {
      return {x - other.x, y - other.y, z - other.z, w - other.w};
    }

    Vector4Base operator*(T scalar) const {
      return {scalar * x, scalar * y, scalar * z, scalar * w};
    }

    Vector4Base operator*(const Vector4Base<T>& other) const {
      Vector4Base result;
      for (uint32_t i = 0; i < 4; i++)
        result[i] = data[i] * other.data[i];
      return result;
    }

    Vector4Base operator/(const Vector4Base<T>& other) const {
      Vector4Base result;
      for (uint32_t i = 0; i < 4; i++)
        result[i] = data[i] / other.data[i];
      return result;
    }

    Vector4Base operator/(T scalar) const {
      return {x / scalar, y / scalar, z / scalar, w / scalar};
    }

    Vector4Base& operator+=(const Vector4Base<T>& other) {
      x += other.x; y += other.y; z += other.z; w += other.w; return *this;
    }
    Vector4Base& operator-=(const Vector4Base<T>& other) {
      x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this;
    }
    Vector4Base& operator*=(T scalar) {
      x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this;
    }
    Vector4Base& operator/=(T scalar) {
      x /= scalar; y /= scalar; z /= scalar; w /= scalar; return *this;
    }
  };

  // FLOAT TEMPLATES
  // SSE, which depends CPU support, or
  // scalar fallback.

#if defined(__SSE2__) || defined(__SSE4_1__)
  // SSE2/SSE4.1 implementation (uses unaligned loads/stores for safety)
  template <>
  struct Vector4Base<float> {
    union {
      float data[4];
      struct { float x, y, z, w; };
      struct { float r, g, b, a; };
    };

    Vector4Base() : x{0.0f}, y{0.0f}, z{0.0f}, w{0.0f} { }
    Vector4Base(float splat) : x(splat), y(splat), z(splat), w(splat) { }
    Vector4Base(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) { }
    Vector4Base(const float xyzw[4]) : x(xyzw[0]), y(xyzw[1]), z(xyzw[2]), w(xyzw[3]) { }

    Vector4Base(const Vector4Base<float>& other) = default;
    Vector4Base& operator=(const Vector4Base<float>& other) = default;

    inline       float& operator[](size_t index)       { return data[index]; }
    inline const float& operator[](size_t index) const { return data[index]; }

    bool operator==(const Vector4Base<float>& other) const {
      __m128 a = _mm_loadu_ps(data);
      __m128 b = _mm_loadu_ps(other.data);
      __m128 cmp = _mm_cmpeq_ps(a, b);
      return _mm_movemask_ps(cmp) == 0xF;
    }
    bool operator!=(const Vector4Base<float>& other) const { return !operator==(other); }

    Vector4Base operator-() const {
      Vector4Base result;
      _mm_storeu_ps(result.data, _mm_sub_ps(_mm_setzero_ps(), _mm_loadu_ps(data)));
      return result;
    }

    Vector4Base operator+(const Vector4Base<float>& o) const {
      Vector4Base result;
      _mm_storeu_ps(result.data, _mm_add_ps(_mm_loadu_ps(data), _mm_loadu_ps(o.data)));
      return result;
    }

    Vector4Base operator-(const Vector4Base<float>& o) const {
      Vector4Base result;
      _mm_storeu_ps(result.data, _mm_sub_ps(_mm_loadu_ps(data), _mm_loadu_ps(o.data)));
      return result;
    }

    Vector4Base operator*(float scalar) const {
      Vector4Base result;
      __m128 s = _mm_set1_ps(scalar);
      _mm_storeu_ps(result.data, _mm_mul_ps(_mm_loadu_ps(data), s));
      return result;
    }

    Vector4Base operator*(const Vector4Base<float>& o) const {
      Vector4Base result;
      _mm_storeu_ps(result.data, _mm_mul_ps(_mm_loadu_ps(data), _mm_loadu_ps(o.data)));
      return result;
    }

    Vector4Base operator/(const Vector4Base<float>& o) const {
      Vector4Base result;
      _mm_storeu_ps(result.data, _mm_div_ps(_mm_loadu_ps(data), _mm_loadu_ps(o.data)));
      return result;
    }

    Vector4Base operator/(float scalar) const {
      return (*this) * (1.0f / scalar);
    }

    Vector4Base& operator+=(const Vector4Base<float>& o) {
      _mm_storeu_ps(data, _mm_add_ps(_mm_loadu_ps(data), _mm_loadu_ps(o.data)));
      return *this;
    }

    Vector4Base& operator-=(const Vector4Base<float>& o) {
      _mm_storeu_ps(data, _mm_sub_ps(_mm_loadu_ps(data), _mm_loadu_ps(o.data)));
      return *this;
    }

    Vector4Base& operator*=(float scalar) {
      __m128 s = _mm_set1_ps(scalar);
      _mm_storeu_ps(data, _mm_mul_ps(_mm_loadu_ps(data), s));
      return *this;
    }

    Vector4Base& operator/=(float scalar) {
      return (*this) *= (1.0f / scalar);
    }
  };
#else
  // Scalar implemenation
  template <>
  struct Vector4Base<float> {
    union {
      float data[4];
      struct { float x, y, z, w; };
      struct { float r, g, b, a; };
    };

    Vector4Base() : x{0.0f}, y{0.0f}, z{0.0f}, w{0.0f} { }
    Vector4Base(float splat) : x(splat), y(splat), z(splat), w(splat) { }
    Vector4Base(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) { }
    Vector4Base(const float xyzw[4]) : x(xyzw[0]), y(xyzw[1]), z(xyzw[2]), w(xyzw[3]) { }

    Vector4Base(const Vector4Base<float>& other) = default;
    Vector4Base& operator=(const Vector4Base<float>& other) = default;

    inline       float& operator[](size_t index)       { return data[index]; }
    inline const float& operator[](size_t index) const { return data[index]; }

    bool operator==(const Vector4Base<float>& other) const {
      return (x == other.x && y == other.y && z == other.z && w == other.w);
    }
    bool operator!=(const Vector4Base<float>& other) const { return !operator==(other); }

    Vector4Base operator-() const { return {-x, -y, -z, -w}; }
    Vector4Base operator+(const Vector4Base<float>& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vector4Base operator-(const Vector4Base<float>& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vector4Base operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar, w * scalar}; }
    Vector4Base operator*(const Vector4Base<float>& o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
    Vector4Base operator/(const Vector4Base<float>& o) const { return {x / o.x, y / o.y, z / o.z, w / o.w}; }
    Vector4Base operator/(float scalar) const { return (*this) * (1.0f / scalar); }

    Vector4Base& operator+=(const Vector4Base<float>& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
    Vector4Base& operator-=(const Vector4Base<float>& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
    Vector4Base& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }
    Vector4Base& operator/=(float scalar) { return (*this) *= (1.0f / scalar); }
  };
#endif

  // NON-MEMBER FUNCTIONS

  template <typename T>
  inline Vector4Base<T> operator*(T scalar, const Vector4Base<T>& vector) {
    return vector * scalar;
  }

  // Optimized dot product for float: prefer SSE4.1 dp, else SSE2 reduction, else scalar.
#if defined(__SSE4_1__)
  inline float dot(const Vector4Base<float>& a, const Vector4Base<float>& b) {
    __m128 res = _mm_dp_ps(_mm_loadu_ps(a.data), _mm_loadu_ps(b.data), 0xF1);
    float out;
    _mm_store_ss(&out, res);
    return out;
  }
#elif defined(__SSE2__)
  inline float dot(const Vector4Base<float>& a, const Vector4Base<float>& b) {
    __m128 mul = _mm_mul_ps(_mm_loadu_ps(a.data), _mm_loadu_ps(b.data));
    __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(1, 0, 3, 2));
    __m128 sums = _mm_add_ps(mul, shuf);
    shuf = _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 total = _mm_add_ps(sums, shuf);
    float out;
    _mm_store_ss(&out, total);
    return out;
  }
#else
  template <typename T>
  inline float dot(const Vector4Base<T>& a, const Vector4Base<T>& b) {
    return float(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
  }
#endif

  template <typename T>
  inline T lengthSqr(const Vector4Base<T>& a) { return dot(a, a); }

  template <typename T>
  inline float length(const Vector4Base<T>& a) { return std::sqrt(float(lengthSqr(a))); }

  template <typename T>
  inline Vector4Base<T> normalize(const Vector4Base<T>& a) { return a * T(1.0f / length(a)); }

  template <typename T>
  inline std::ostream& operator<<(std::ostream& os, const Vector4Base<T>& v) {
    return os << "Vector4(" << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << ")";
  }

  using Vector4  = Vector4Base<float>;
  using Vector4i = Vector4Base<int>;

  static_assert(sizeof(Vector4)  == sizeof(float) * 4);
  static_assert(sizeof(Vector4i) == sizeof(int)   * 4);

  // replaceNaN: SSE2 implementation or scalar fallback
#if defined(__SSE2__) || defined(__SSE4_1__)
  inline Vector4 replaceNaN(Vector4 a) {
    Vector4 result;
    __m128 value = _mm_loadu_ps(a.data);
    __m128 mask  = _mm_cmpeq_ps(value, value); // NaN != NaN -> mask zero
           value = _mm_and_ps(value, mask);    // zero-out NaNs
    _mm_storeu_ps(result.data, value);
    return result;
  }
#else
  inline Vector4 replaceNaN(Vector4 a) {
    for (int i = 0; i < 4; i++)
      a[i] = std::isnan(a[i]) ? 0.0f : a[i];
    return a;
  }
#endif

}