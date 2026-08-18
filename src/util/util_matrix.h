#pragma once

#include <optional>
#include <iosfwd>
#include <cstddef>

#include "util_vector.h"

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

  class Matrix4 {

    public:

    // Identity Constructor
    inline Matrix4() {
      data[0] = Vector4{ 1.0f, 0.0f, 0.0f, 0.0f };
      data[1] = Vector4{ 0.0f, 1.0f, 0.0f, 0.0f };
      data[2] = Vector4{ 0.0f, 0.0f, 1.0f, 0.0f };
      data[3] = Vector4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    // Scalar Constructor
    inline explicit Matrix4(float x) {
      data[0] = Vector4{ x,    0.0f, 0.0f, 0.0f };
      data[1] = Vector4{ 0.0f, x,    0.0f, 0.0f };
      data[2] = Vector4{ 0.0f, 0.0f, x,    0.0f };
      data[3] = Vector4{ 0.0f, 0.0f, 0.0f, x    };
    }

    inline Matrix4(
      const Vector4& v0,
      const Vector4& v1,
      const Vector4& v2,
      const Vector4& v3) {
      data[0] = v0;
      data[1] = v1;
      data[2] = v2;
      data[3] = v3;
    }

    inline Matrix4(const float matrix[4][4]) {
      data[0] = Vector4{ matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3] };
      data[1] = Vector4{ matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3] };
      data[2] = Vector4{ matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3] };
      data[3] = Vector4{ matrix[3][0], matrix[3][1], matrix[3][2], matrix[3][3] };
    }

    Matrix4(const Matrix4& other) = default;
    Matrix4& operator=(const Matrix4& other) = default;

    Vector4& operator[](size_t index);
    const Vector4& operator[](size_t index) const;

    [[nodiscard]] bool operator==(const Matrix4& m2) const;
    [[nodiscard]] bool operator!=(const Matrix4& m2) const;

    [[nodiscard]] Matrix4 operator+(const Matrix4& other) const;
    [[nodiscard]] Matrix4 operator-(const Matrix4& other) const;

    [[nodiscard]] Matrix4 operator*(const Matrix4& m2) const;
    [[nodiscard]] Vector4 operator*(const Vector4& v) const;
    [[nodiscard]] Matrix4 operator*(float scalar) const;

    [[nodiscard]] Matrix4 operator/(float scalar) const;

    Matrix4& operator+=(const Matrix4& other);
    Matrix4& operator-=(const Matrix4& other);
    Matrix4& operator*=(const Matrix4& other);

    Vector4 data[4];

  };

  static_assert(sizeof(Matrix4) == sizeof(Vector4) * 4);

  inline Matrix4 operator*(float scalar, const Matrix4& m) { return m * scalar; }

  [[nodiscard]] Matrix4 transpose(const Matrix4& m);

  [[nodiscard]] float determinant(const Matrix4& m);

  [[nodiscard]] std::optional<Matrix4> tryInverse(const Matrix4& m);

  [[nodiscard]] Matrix4 inverse(const Matrix4& m);

  [[nodiscard]] Matrix4 hadamardProduct(const Matrix4& a, const Matrix4& b);

  std::ostream& operator<<(std::ostream& os, const Matrix4& m);

}