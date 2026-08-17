#include "util_matrix.h"

#include <cmath>
#include <limits>

#if defined(__SSE4_1__) || defined(__SSE2__)
  #include <emmintrin.h> // SSE2
#endif
#if defined(__SSE4_1__)
  #include <smmintrin.h> // SSE4.1 (for _mm_dp_ps)
#endif

namespace dxvk {

#if defined(__SSE2__)

  // NOTE: Use unaligned loads/stores (_mm_loadu_ps/_mm_storeu_ps) for safety
  // because Matrix4/Vector4 may be placed in memory without guaranteed
  // 16-byte alignment in all places (mapped buffers, packed structs, etc).

  Vector4& Matrix4::operator[](size_t index)       { return data[index]; }
  const Vector4& Matrix4::operator[](size_t index) const { return data[index]; }

  bool Matrix4::operator==(const Matrix4& m2) const {
  #if defined(__SSE4_1__)
    __m128i r0 = _mm_castps_si128(_mm_cmpeq_ps(_mm_loadu_ps(&data[0].x), _mm_loadu_ps(&m2.data[0].x)));
    __m128i r1 = _mm_castps_si128(_mm_cmpeq_ps(_mm_loadu_ps(&data[1].x), _mm_loadu_ps(&m2.data[1].x)));
    __m128i r2 = _mm_castps_si128(_mm_cmpeq_ps(_mm_loadu_ps(&data[2].x), _mm_loadu_ps(&m2.data[2].x)));
    __m128i r3 = _mm_castps_si128(_mm_cmpeq_ps(_mm_loadu_ps(&data[3].x), _mm_loadu_ps(&m2.data[3].x)));

    __m128i and01 = _mm_and_si128(r0, r1);
    __m128i and23 = _mm_and_si128(r2, r3);
    __m128i final_mask = _mm_and_si128(and01, and23);

    return _mm_movemask_ps(_mm_castsi128_ps(final_mask)) == 0xF;
  #else
    for (uint32_t i = 0; i < 4; i++) {
      __m128 r1 = _mm_loadu_ps(&data[i].x);
      __m128 r2 = _mm_loadu_ps(&m2.data[i].x);
      if (_mm_movemask_ps(_mm_cmpeq_ps(r1, r2)) != 0xF)
        return false;
    }
    return true;
  #endif
  }

  bool Matrix4::operator!=(const Matrix4& m2) const { return !operator==(m2); }

  Matrix4 Matrix4::operator+(const Matrix4& other) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++) {
      _mm_storeu_ps(&mat.data[i].x, _mm_add_ps(_mm_loadu_ps(&data[i].x), _mm_loadu_ps(&other.data[i].x)));
    }
    return mat;
  }

  Matrix4 Matrix4::operator-(const Matrix4& other) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++) {
      _mm_storeu_ps(&mat.data[i].x, _mm_sub_ps(_mm_loadu_ps(&data[i].x), _mm_loadu_ps(&other.data[i].x)));
    }
    return mat;
  }

  Matrix4 Matrix4::operator*(const Matrix4& m2) const {
    Matrix4 result;
    __m128 rowB0 = _mm_loadu_ps(&m2.data[0].x);
    __m128 rowB1 = _mm_loadu_ps(&m2.data[1].x);
    __m128 rowB2 = _mm_loadu_ps(&m2.data[2].x);
    __m128 rowB3 = _mm_loadu_ps(&m2.data[3].x);

    for (uint32_t i = 0; i < 4; i++) {
      __m128 rowA = _mm_loadu_ps(&data[i].x);

      __m128 e0 = _mm_shuffle_ps(rowA, rowA, _MM_SHUFFLE(0, 0, 0, 0));
      __m128 e1 = _mm_shuffle_ps(rowA, rowA, _MM_SHUFFLE(1, 1, 1, 1));
      __m128 e2 = _mm_shuffle_ps(rowA, rowA, _MM_SHUFFLE(2, 2, 2, 2));
      __m128 e3 = _mm_shuffle_ps(rowA, rowA, _MM_SHUFFLE(3, 3, 3, 3));

      __m128 v0 = _mm_mul_ps(e0, rowB0);
      __m128 v1 = _mm_mul_ps(e1, rowB1);
      __m128 v2 = _mm_mul_ps(e2, rowB2);
      __m128 v3 = _mm_mul_ps(e3, rowB3);

      __m128 sum01 = _mm_add_ps(v0, v1);
      __m128 sum23 = _mm_add_ps(v2, v3);
      _mm_storeu_ps(&result.data[i].x, _mm_add_ps(sum01, sum23));
    }
    return result;
  }

  Vector4 Matrix4::operator*(const Vector4& v) const {
    Vector4 result;
    __m128 vec = _mm_loadu_ps(&v.x);

  #if defined(__SSE4_1__)
    __m128 row0 = _mm_loadu_ps(&data[0].x);
    __m128 row1 = _mm_loadu_ps(&data[1].x);
    __m128 row2 = _mm_loadu_ps(&data[2].x);
    __m128 row3 = _mm_loadu_ps(&data[3].x);

    float x = _mm_cvtss_f32(_mm_dp_ps(row0, vec, 0xF1));
    float y = _mm_cvtss_f32(_mm_dp_ps(row1, vec, 0xF1));
    float z = _mm_cvtss_f32(_mm_dp_ps(row2, vec, 0xF1));
    float w = _mm_cvtss_f32(_mm_dp_ps(row3, vec, 0xF1));

    result.x = x; result.y = y; result.z = z; result.w = w;
  #else
    __m128 e0 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 e1 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(1, 1, 1, 1));
    __m128 e2 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(2, 2, 2, 2));
    __m128 e3 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(3, 3, 3, 3));

    __m128 row0 = _mm_loadu_ps(&data[0].x);
    __m128 row1 = _mm_loadu_ps(&data[1].x);
    __m128 row2 = _mm_loadu_ps(&data[2].x);
    __m128 row3 = _mm_loadu_ps(&data[3].x);

    __m128 v0 = _mm_mul_ps(e0, row0);
    __m128 v1 = _mm_mul_ps(e1, row1);
    __m128 v2 = _mm_mul_ps(e2, row2);
    __m128 v3 = _mm_mul_ps(e3, row3);

    __m128 sum01 = _mm_add_ps(v0, v1);
    __m128 sum23 = _mm_add_ps(v2, v3);
    _mm_storeu_ps(&result.x, _mm_add_ps(sum01, sum23));
  #endif

    return result;
  }

  Matrix4 Matrix4::operator*(float scalar) const {
    Matrix4 result;
    __m128 s = _mm_set1_ps(scalar);
    for (uint32_t i = 0; i < 4; i++) {
      _mm_storeu_ps(&result.data[i].x, _mm_mul_ps(_mm_loadu_ps(&data[i].x), s));
    }
    return result;
  }

  Matrix4 Matrix4::operator/(float scalar) const {
    return (*this) * (1.0f / scalar);
  }

  Matrix4 hadamardProduct(const Matrix4& a, const Matrix4& b) {
    Matrix4 result;
    for (uint32_t i = 0; i < 4; i++) {
      _mm_storeu_ps(&result.data[i].x, _mm_mul_ps(_mm_loadu_ps(&a.data[i].x), _mm_loadu_ps(&b.data[i].x)));
    }
    return result;
  }

  Matrix4& Matrix4::operator+=(const Matrix4& other) {
    for (uint32_t i = 0; i < 4; i++) {
      _mm_storeu_ps(&data[i].x, _mm_add_ps(_mm_loadu_ps(&data[i].x), _mm_loadu_ps(&other.data[i].x)));
    }
    return *this;
  }

  Matrix4& Matrix4::operator-=(const Matrix4& other) {
    for (uint32_t i = 0; i < 4; i++) {
      _mm_storeu_ps(&data[i].x, _mm_sub_ps(_mm_loadu_ps(&data[i].x), _mm_loadu_ps(&other.data[i].x)));
    }
    return *this;
  }

  Matrix4& Matrix4::operator*=(const Matrix4& other) {
    *this = (*this) * other;
    return *this;
  }

  Matrix4 transpose(const Matrix4& m) {
    Matrix4 result;
    __m128 r0 = _mm_loadu_ps(&m.data[0].x);
    __m128 r1 = _mm_loadu_ps(&m.data[1].x);
    __m128 r2 = _mm_loadu_ps(&m.data[2].x);
    __m128 r3 = _mm_loadu_ps(&m.data[3].x);

    __m128 t0 = _mm_unpacklo_ps(r0, r1);
    __m128 t1 = _mm_unpackhi_ps(r0, r1);
    __m128 t2 = _mm_unpacklo_ps(r2, r3);
    __m128 t3 = _mm_unpackhi_ps(r2, r3);

    _mm_storeu_ps(&result.data[0].x, _mm_movelh_ps(t0, t2));
    _mm_storeu_ps(&result.data[1].x, _mm_movehl_ps(t2, t0));
    _mm_storeu_ps(&result.data[2].x, _mm_movelh_ps(t1, t3));
    _mm_storeu_ps(&result.data[3].x, _mm_movehl_ps(t3, t1));

    return result;
  }

  float determinant(const Matrix4& m) {
    __m128 r0 = _mm_loadu_ps(&m.data[0].x);
    __m128 r1 = _mm_loadu_ps(&m.data[1].x);
    __m128 r2 = _mm_loadu_ps(&m.data[2].x);
    __m128 r3 = _mm_loadu_ps(&m.data[3].x);

    __m128 r2_wzx_y = _mm_shuffle_ps(r2, r2, _MM_SHUFFLE(1, 0, 2, 3));
    __m128 r3_zwy_x = _mm_shuffle_ps(r3, r3, _MM_SHUFFLE(0, 1, 3, 2));
    __m128 r2_wzy_x = _mm_shuffle_ps(r2, r2, _MM_SHUFFLE(0, 1, 2, 3));
    __m128 r3_zwx_y = _mm_shuffle_ps(r3, r3, _MM_SHUFFLE(1, 0, 3, 2));

    __m128 sub_cofactors = _mm_sub_ps(_mm_mul_ps(r2_wzx_y, r3_zwy_x), _mm_mul_ps(r2_wzy_x, r3_zwx_y));

    __m128 c5 = _mm_shuffle_ps(sub_cofactors, sub_cofactors, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 c4 = _mm_shuffle_ps(sub_cofactors, sub_cofactors, _MM_SHUFFLE(1, 1, 1, 1));
    __m128 c3 = _mm_shuffle_ps(sub_cofactors, sub_cofactors, _MM_SHUFFLE(2, 2, 2, 2));

    __m128 r1_yxx_w = _mm_shuffle_ps(r1, r1, _MM_SHUFFLE(3, 0, 0, 1));
    __m128 r1_zzy_z = _mm_shuffle_ps(r1, r1, _MM_SHUFFLE(2, 1, 2, 2));
    __m128 r1_www_x = _mm_shuffle_ps(r1, r1, _MM_SHUFFLE(0, 3, 3, 3));

    __m128 cofactors = _mm_sub_ps(_mm_mul_ps(r1_yxx_w, c5), _mm_mul_ps(r1_zzy_z, c4));
    cofactors = _mm_add_ps(cofactors, _mm_mul_ps(r1_www_x, c3));

    __m128 det_vec;
  #if defined(__SSE4_1__)
    det_vec = _mm_dp_ps(r0, cofactors, 0xF1);
  #else
    __m128 mul_rows = _mm_mul_ps(r0, cofactors);
    __m128 shuf = _mm_shuffle_ps(mul_rows, mul_rows, _MM_SHUFFLE(1, 0, 3, 2));
    __m128 sums = _mm_add_ps(mul_rows, shuf);
    shuf = _mm_shuffle_ps(sums, sums, _MM_SHUFFLE(2, 3, 0, 1));
    det_vec = _mm_add_ps(sums, shuf);
  #endif

    float det;
    _mm_store_ss(&det, det_vec);
    return det;
  }

  std::optional<Matrix4> tryInverse(const Matrix4& m) {
    // Same algorithm as scalar fallback but using Vector4 helpers for building the adjugate.
    // Compute cofactors using scalar ops on elements (keeps numerical consistency with scalar path).
    float coef00    = m[2][2] * m[3][3] - m[3][2] * m[2][3];
    float coef02    = m[1][2] * m[3][3] - m[3][2] * m[1][3];
    float coef03    = m[1][2] * m[2][3] - m[2][2] * m[1][3];
    float coef04    = m[2][1] * m[3][3] - m[3][1] * m[2][3];
    float coef06    = m[1][1] * m[3][3] - m[3][1] * m[1][3];
    float coef07    = m[1][1] * m[2][3] - m[2][1] * m[1][3];
    float coef08    = m[2][1] * m[3][2] - m[3][1] * m[2][2];
    float coef10    = m[1][1] * m[3][2] - m[3][1] * m[1][2];
    float coef11    = m[1][1] * m[2][2] - m[2][1] * m[1][2];
    float coef12    = m[2][0] * m[3][3] - m[3][0] * m[2][3];
    float coef14    = m[1][0] * m[3][3] - m[3][0] * m[1][3];
    float coef15    = m[1][0] * m[2][3] - m[2][0] * m[1][3];
    float coef16    = m[2][0] * m[3][2] - m[3][0] * m[2][2];
    float coef18    = m[1][0] * m[3][2] - m[3][0] * m[1][2];
    float coef19    = m[1][0] * m[2][2] - m[2][0] * m[1][2];
    float coef20    = m[2][0] * m[3][1] - m[3][0] * m[2][1];
    float coef22    = m[1][0] * m[3][1] - m[3][0] * m[1][1];
    float coef23    = m[1][0] * m[2][1] - m[2][0] * m[1][1];

    Vector4 fac0    = { coef00, coef00, coef02, coef03 };
    Vector4 fac1    = { coef04, coef04, coef06, coef07 };
    Vector4 fac2    = { coef08, coef08, coef10, coef11 };
    Vector4 fac3    = { coef12, coef12, coef14, coef15 };
    Vector4 fac4    = { coef16, coef16, coef18, coef19 };
    Vector4 fac5    = { coef20, coef20, coef22, coef23 };

    Vector4 vec0    = { m[1][0], m[0][0], m[0][0], m[0][0] };
    Vector4 vec1    = { m[1][1], m[0][1], m[0][1], m[0][1] };
    Vector4 vec2    = { m[1][2], m[0][2], m[0][2], m[0][2] };
    Vector4 vec3    = { m[1][3], m[0][3], m[0][3], m[0][3] };

    Vector4 inv0    = { vec1 * fac0 - vec2 * fac1 + vec3 * fac2 };
    Vector4 inv1    = { vec0 * fac0 - vec2 * fac3 + vec3 * fac4 };
    Vector4 inv2    = { vec0 * fac1 - vec1 * fac3 + vec3 * fac5 };
    Vector4 inv3    = { vec0 * fac2 - vec1 * fac4 + vec2 * fac5 };

    Vector4 signA   = { +1, -1, +1, -1 };
    Vector4 signB   = { -1, +1, -1, +1 };
    Matrix4 inverse = { inv0 * signA, inv1 * signB, inv2 * signA, inv3 * signB };

    Vector4 row0    = { inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0] };

    Vector4 dot0    = { m[0] * row0 };
    float dot1      = (dot0.x + dot0.y) + (dot0.z + dot0.w);

    float maxAbs = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) {
      const float* row = &m.data[i].x;
      for (uint32_t j = 0; j < 4; ++j)
        maxAbs = std::max(maxAbs, std::abs(row[j]));
    }

    if (std::abs(dot1) < 1e-9f || std::abs(dot1) < 1e-6f * std::max(1.0f, maxAbs))
      return std::nullopt;

    return std::make_optional(inverse * (1.0f / dot1));
  }

  Matrix4 inverse(const Matrix4& m) {
    return tryInverse(m).value_or(m);
  }

  std::ostream& operator<<(std::ostream& os, const Matrix4& m) {
    os << "Matrix4(";
    for (uint32_t i = 0; i < 4; i++) {
      os << "\n\t" << m[i];
      if (i < 3) os << ", ";
    }
    os << "\n)";
    return os;
  }

#else

  Vector4& Matrix4::operator[](size_t index)       { return data[index]; }
  const Vector4& Matrix4::operator[](size_t index) const { return data[index]; }

  bool Matrix4::operator==(const Matrix4& m2) const {
    const Matrix4& m1 = *this;
    for (uint32_t i = 0; i < 4; i++) {
      if (m1[i] != m2[i])
        return false;
    }
    return true;
  }

  bool Matrix4::operator!=(const Matrix4& m2) const { return !operator==(m2); }

  Matrix4 Matrix4::operator+(const Matrix4& other) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++)
      mat[i] = data[i] + other.data[i];
    return mat;
  }

  Matrix4 Matrix4::operator-(const Matrix4& other) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++)
      mat[i] = data[i] - other.data[i];
    return mat;
  }

  Matrix4 Matrix4::operator*(const Matrix4& m2) const {
    const Matrix4& m1 = *this;

    const Vector4 srcA0 = { m1[0] };
    const Vector4 srcA1 = { m1[1] };
    const Vector4 srcA2 = { m1[2] };
    const Vector4 srcA3 = { m1[3] };

    const Vector4 srcB0 = { m2[0] };
    const Vector4 srcB1 = { m2[1] };
    const Vector4 srcB2 = { m2[2] };
    const Vector4 srcB3 = { m2[3] };

    Matrix4 result;
    result[0] = srcA0 * srcB0[0] + srcA1 * srcB0[1] + srcA2 * srcB0[2] + srcA3 * srcB0[3];
    result[1] = srcA0 * srcB1[0] + srcA1 * srcB1[1] + srcA2 * srcB1[2] + srcA3 * srcB1[3];
    result[2] = srcA0 * srcB2[0] + srcA1 * srcB2[1] + srcA2 * srcB2[2] + srcA3 * srcB2[3];
    result[3] = srcA0 * srcB3[0] + srcA1 * srcB3[1] + srcA2 * srcB3[2] + srcA3 * srcB3[3];
    return result;
  }

  Vector4 Matrix4::operator*(const Vector4& v) const {
    const Matrix4& m = *this;

    const Vector4 mul0 = { m[0] * v[0] };
    const Vector4 mul1 = { m[1] * v[1] };
    const Vector4 mul2 = { m[2] * v[2] };
    const Vector4 mul3 = { m[3] * v[3] };

    const Vector4 add0 = { mul0 + mul1 };
    const Vector4 add1 = { mul2 + mul3 };

    return add0 + add1;
  }

  Matrix4 Matrix4::operator*(float scalar) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++)
      mat[i] = data[i] * scalar;
    return mat;
  }

  Matrix4 Matrix4::operator/(float scalar) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++)
      mat[i] = data[i] / scalar;
    return mat;
  }

  Matrix4& Matrix4::operator+=(const Matrix4& other) {
    for (uint32_t i = 0; i < 4; i++)
      data[i] += other.data[i];
    return *this;
  }

  Matrix4& Matrix4::operator-=(const Matrix4& other) {
    for (uint32_t i = 0; i < 4; i++)
      data[i] -= other.data[i];
    return *this;
  }

  Matrix4& Matrix4::operator*=(const Matrix4& other) {
    return (*this = (*this) * other);
  }

  Matrix4 transpose(const Matrix4& m) {
    Matrix4 result;
    for (uint32_t i = 0; i < 4; i++) {
      for (uint32_t j = 0; j < 4; j++)
        result[i][j] = m.data[j][i];
    }
    return result;
  }

  float determinant(const Matrix4& m) {
    float coef00    =  m[2][2] * m[3][3] - m[3][2] * m[2][3];
    float coef02    =  m[1][2] * m[3][3] - m[3][2] * m[1][3];
    float coef03    =  m[1][2] * m[2][3] - m[2][2] * m[1][3];

    float coef04    =  m[2][1] * m[3][3] - m[3][1] * m[2][3];
    float coef06    =  m[1][1] * m[3][3] - m[3][1] * m[1][3];
    float coef07    =  m[1][1] * m[2][3] - m[2][1] * m[1][3];

    float coef08    =  m[2][1] * m[3][2] - m[3][1] * m[2][2];
    float coef10    =  m[1][1] * m[3][2] - m[3][1] * m[1][2];
    float coef11    =  m[1][1] * m[2][2] - m[2][1] * m[1][2];

    float coef12    =  m[2][0] * m[3][3] - m[3][0] * m[2][3];
    float coef14    =  m[1][0] * m[3][3] - m[3][0] * m[1][3];
    float coef15    =  m[1][0] * m[2][3] - m[2][0] * m[1][3];

    float coef16    =  m[2][0] * m[3][2] - m[3][0] * m[2][2];
    float coef18    =  m[1][0] * m[3][2] - m[3][0] * m[1][2];
    float coef19    =  m[1][0] * m[2][2] - m[2][0] * m[1][2];

    float coef20    =  m[2][0] * m[3][1] - m[3][0] * m[2][1];
    float coef22    =  m[1][0] * m[3][1] - m[3][0] * m[1][1];
    float coef23    =  m[1][0] * m[2][1] - m[2][0] * m[1][1];

    Vector4 fac0    = { coef00, coef00, coef02, coef03 };
    Vector4 fac1    = { coef04, coef04, coef06, coef07 };
    Vector4 fac2    = { coef08, coef08, coef10, coef11 };
    Vector4 fac3    = { coef12, coef12, coef14, coef15 };
    Vector4 fac4    = { coef16, coef16, coef18, coef19 };
    Vector4 fac5    = { coef20, coef20, coef22, coef23 };

    Vector4 vec0    = { m[1][0], m[0][0], m[0][0], m[0][0] };
    Vector4 vec1    = { m[1][1], m[0][1], m[0][1], m[0][1] };
    Vector4 vec2    = { m[1][2], m[0][2], m[0][2], m[0][2] };
    Vector4 vec3    = { m[1][3], m[0][3], m[0][3], m[0][3] };

    Vector4 inv0    = { vec1 * fac0 - vec2 * fac1 + vec3 * fac2 };
    Vector4 inv1    = { vec0 * fac0 - vec2 * fac3 + vec3 * fac4 };
    Vector4 inv2    = { vec0 * fac1 - vec1 * fac3 + vec3 * fac5 };
    Vector4 inv3    = { vec0 * fac2 - vec1 * fac4 + vec2 * fac5 };

    Vector4 signA   = { +1, -1, +1, -1 };
    Vector4 signB   = { -1, +1, -1, +1 };
    Matrix4 inverse = { inv0 * signA, inv1 * signB, inv2 * signA, inv3 * signB };

    Vector4 row0    = { inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0] };

    Vector4 dot0    = { m[0] * row0 };
    float dot1      = (dot0.x + dot0.y) + (dot0.z + dot0.w);

    float maxAbs = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) {
      const float* row = &m.data[i].x;
      for (uint32_t j = 0; j < 4; ++j)
        maxAbs = std::max(maxAbs, std::abs(row[j]));
    }

    if (std::abs(dot1) < 1e-9f || std::abs(dot1) < 1e-6f * std::max(1.0f, maxAbs))
      return std::nullopt;

    return std::make_optional(inverse * (1.0f / dot1));
  }

  Matrix4 inverse(const Matrix4& m) {
    return tryInverse(m).value_or(m);
  }

  Matrix4 hadamardProduct(const Matrix4& a, const Matrix4& b) {
    Matrix4 result;
    for (uint32_t i = 0; i < 4; i++)
      result[i] = a[i] * b[i];
    return result;
  }

  std::ostream& operator<<(std::ostream& os, const Matrix4& m) {
    os << "Matrix4(";
    for (uint32_t i = 0; i < 4; i++) {
      os << "\n\t" << m[i];
      if (i < 3)
        os << ", ";
    }
    os << "\n)";
    return os;
  }

#endif

}