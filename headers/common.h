#pragma once

#define FUNC_DECL

#include <immintrin.h>
#include <atomic>
#include <numeric>

#if !defined(TC_INCLUDED)

#ifdef _WIN64
#define TC_FORCE_INLINE __forceinline
#else
#define TC_FORCE_INLINE inline __attribute__((always_inline))
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <array>
#include <vector>

using float32 = float;
using float64 = double;
using int32 = int;
using uint64 = unsigned long long;
using uint8 = unsigned char;
using uint16 = unsigned short;

#if defined(TLANG_GPU)
#include <cuda_runtime.h>
#undef FUNC_DECL
#define FUNC_DECL __host__ __device__
#endif

#endif

namespace taichi {
namespace Tlang {

struct Context {
  static constexpr int max_num_buffers = 16;
  static constexpr int max_num_parameters = 16;
  static constexpr int max_num_ranges = 16;
  using Buffer = void *;
  Buffer buffers[max_num_buffers];
  double parameters[max_num_parameters];
  uint64 ranges[max_num_ranges];

  Context() {
    std::memset(buffers, 0, sizeof(buffers));
    std::memset(parameters, 0, sizeof(parameters));
    std::memset(ranges, 0, sizeof(ranges));
  }

  Context(void *x, void *y, void *z, uint64 n) : Context() {
    buffers[0] = x;
    buffers[1] = y;
    buffers[2] = z;
    ranges[0] = n;
  }

  template <typename T>
  FUNC_DECL T *get_buffer(int i) {
    return reinterpret_cast<T *>(buffers[i]);
  }

  template <typename T>
  FUNC_DECL T &get_parameter(int i) {
    return *reinterpret_cast<T *>(&parameters[i]);
  }

  FUNC_DECL uint64 &get_range(int i) {
    return ranges[i];
  }
};

// Virtual Vectors

template <int dim, typename T>
struct VV {  // Virtual Vector
  T d[dim];

  VV() {
  }

  VV(T v) {
    for (int i = 0; i < dim; i++) {
      d[i] = v;
    }
  }

  VV(std::array<T, dim> val) {
    for (int i = 0; i < dim; i++) {
      d[i] = val[i];
    }
  }

  T &operator[](int i) {
    return d[i];
  }

  const T &operator[](int i) const {
    return d[i];
  }

  void print() const {
    std::cout << "[";
    for (int i = 0; i < dim; i++) {
      std::cout << d[i] << ", ";
    }
    std::cout << "]\n";
  }
};

#define BINARY_OP(OPNAME, OP)                                               \
  template <int dim, typename T>                                            \
  inline VV<dim, T> operator OP(const VV<dim, T> &a, const VV<dim, T> &b) { \
    VV<dim, T> c;                                                           \
    for (int i = 0; i < dim; i++) {                                         \
      c[i] = a[i] OP b[i];                                                  \
    }                                                                       \
    return c;                                                               \
  }

BINARY_OP(add, +);
BINARY_OP(sub, -);
BINARY_OP(mul, *);
BINARY_OP(div, /);
BINARY_OP(mod, %);

#undef BINARY_OP

template <int dim, typename T>
inline VV<dim, T> min(const VV<dim, T> &a, const VV<dim, T> &b) {
  VV<dim, T> c;
  for (int i = 0; i < dim; i++) {
    c[i] = std::min(a[i], b[i]);
  }
  return c;
}

template <int dim, typename T>
inline VV<dim, T> max(const VV<dim, T> &a, const VV<dim, T> &b) {
  VV<dim, T> c;
  for (int i = 0; i < dim; i++) {
    c[i] = std::max(a[i], b[i]);
  }
  return c;
}

template <int dim, typename T>
inline VV<dim, T> floor(const VV<dim, T> &a) {
  VV<dim, T> c;
  for (int i = 0; i < dim; i++) {
    c[i] = std::floor(a[i]);
  }
  return c;
}

template <typename T, typename G, int dim>
inline VV<dim, T> cast(const VV<dim, G> &a) {
  VV<dim, T> c;
  for (int i = 0; i < dim; i++) {
    c[i] = (T)a[i];
  }
  return c;
}

template <typename T, int dim>
TC_FORCE_INLINE VV<dim, T> land(const VV<dim, T> &a, int b) {
  VV<dim, T> c;
  for (int i = 0; i < dim; i++) {
    c[i] = a[i] & b;
  }
  return c;
}

template <typename T, int dim>
TC_FORCE_INLINE VV<dim, T> shr(const VV<dim, T> &a, int b) {
  VV<dim, T> c;
  for (int i = 0; i < dim; i++) {
    c[i] = a[i] >> b;
  }
  return c;
}

template <int dim, typename T>
inline VV<dim, T> load(T *base_address, VV<dim, int> offsets) {
  VV<dim, T> ret;
  for (int i = 0; i < dim; i++) {
    ret[i] = *(base_address + offsets[i]);
  }
  return ret;
}

template <int dim, typename T>
inline void store(VV<dim, T> a, T *base_address, VV<dim, int> offsets) {
  for (int i = 0; i < dim; i++) {
    *(base_address + offsets[i]) = a[i];
  }
}

template <typename SA, int output_dim, typename T = typename SA::T>
inline VV<output_dim, T> shuffle(SA &a, VV<output_dim, int> offsets) {
  VV<output_dim, T> ret;
  for (int i = 0; i < output_dim; i++) {
    ret[i] = a[offsets[i]];
  }
  return ret;
};

template <typename T_, int num_groups, int num_inputs, int input_group_size>
struct SlowAdapter {
  // static constexpr int num_outputs = num_inputs * input_group_size /
  // output_group_size;
  // static_assert(num_inputs * input_group_size % output_group_size == 0, "");

  using T = T_;
  // static constexpr int num_inputs = 8;
  static constexpr int num_outputs = 8;

  static constexpr int input_dim = num_groups * input_group_size;
  // static constexpr int output_dim = num_groups * output_group_size;

  VV<input_dim, T> inputs[num_inputs];

  template <int i>
  TC_FORCE_INLINE void set(const VV<input_dim, T> &v) {
    static_assert(0 <= i && i < num_inputs, "");
    inputs[i] = v;
  }

  void set(int i, const VV<input_dim, T> &v) {
    inputs[i] = v;
  }

  void shuffle() {
    /*
    constexpr int num_elements = num_inputs * input_group_size;
    static_assert(num_inputs * num_elements * input_group_size ==
                      num_outputs * num_elements * output_group_size,
                  "");

    for (int i = 0; i < num_elements; i++) {
      for (int j = 0; j < num_groups; j++) {
        auto v = inputs[i / input_group_size]
                       [j * input_group_size + i % input_group_size];
        outputs[i / output_group_size]
               [j * output_group_size + i % output_group_size] = v;
      }
    }
    */
  }

  template <int i>
  auto get_input() {
    static_assert(0 <= i && i < num_inputs, "");
    return inputs[i];
  }

  /*
  template <int i>
  auto get() {
    static_assert(0 <= i && i < num_outputs, "");
    return outputs[i];
  }

  auto get(int i) {
    return outputs[i];
  }
  */

  inline T &operator[](int i) {
    return inputs[i / input_dim][i % input_dim];
  }
};
// End Virtual Vectors

template <typename T, int dim>
struct vec_helper;

#define DEFINE_VEC_TYPE(T, dim, _type) \
  template <>                          \
  struct vec_helper<T, dim> {          \
    using type = _type;                \
  };

DEFINE_VEC_TYPE(float32, 1, float32);
DEFINE_VEC_TYPE(int32, 1, int32);
DEFINE_VEC_TYPE(float32, 8, __m256);
DEFINE_VEC_TYPE(int32, 8, __m256i);

template <typename T, int dim>
struct vec;

template <typename T, int dim>
inline vec<T, dim> set1(T);

template <typename T, int dim>
inline vec<T, dim> load(const void *);

template <typename T, int dim>
inline vec<T, dim> gather(const void *, vec<int32, dim>);

template <typename T, int dim>
inline void store(const vec<T, dim> &v, const void *);

template <typename T, int dim>
inline void store(const vec<T, dim> &v, void *, vec<int32, dim>);

template <typename T, int dim>
struct vec {
  using type = typename vec_helper<T, dim>::type;
  type v;
  vec() {
  }
  vec(type v) : v(v) {
  }
  template <typename _T = T>
  vec(std::enable_if_t<!std::is_same<_T, type>::value, T> scalar)
      : v(set1<T, dim>(scalar)) {
  }
  TC_FORCE_INLINE vec(std::array<T, dim> v) {
    for (int i = 0; i < dim; i++) {
      element(i) = v[i];
    }
  }
  operator type() const {
    return v;
  }
  T &operator[](int i) {
    return ((T *)(&v))[i];
  }
  const T &operator[](int i) const {
    return ((T *)(&v))[i];
  }
  void print() {
    std::cout << "[";
    for (int j = 0; j < dim; j++) {
      std::cout << element(j) << ", ";
    }
    std::cout << "]" << std::endl;
  }

  // SIMD types
  template <typename T_ = type>
  std::enable_if_t<!std::is_arithmetic<T_>::value, T> &element(int i) {
    return (*this)[i];
  }

  template <typename T_ = type>
  const std::enable_if_t<!std::is_arithmetic<T_>::value, T> &element(
      int i) const {
    return (*this)[i];
  }

  // scalar types
  template <typename T_ = type>
  typename std::enable_if_t<std::is_arithmetic<T_>::value, T> &element(int i) {
    return v;
  }

  template <typename T_ = type>
  const typename std::enable_if_t<std::is_arithmetic<T_>::value, T> &element(int i) const {
    return v;
  }

  static vec load(T *addr[dim]) {
    vec ret;
    for (int i = 0; i < dim; i++) {
      ret.element(i) = *addr[i];
    }
    return ret;
  }

  static vec load(void *addr) {
    vec ret;
    for (int i = 0; i < dim; i++) {
      ret.d[i] = taichi::Tlang::load<T, dim>(
          (void *)((uint8 *)addr + i * sizeof(vec<T, dim>)));
    }
    return ret;
  }

  static vec load(void *addr, vec<int, dim> offsets) {
    vec ret;
    for (int i = 0; i < dim; i++) {
      ret.d[i] = gather<T, dim>(addr, offsets.d[i]);
    }
    return ret;
  }

  void store(void *addr) {
    taichi::Tlang::store<T, dim>(v, addr);
  }

  void store(void *addr, vec<int32, dim> offsets) {
    taichi::Tlang::store<T, dim>(v, addr, offsets);
  }

  void store(T *addr[dim]) {
    for (int i = 0; i < dim; i++) {
      *addr[i] = element(i);
    }
  }
};

//*****************************************************************************

using float32x1 = vec<float32, 1>;
using int32x1 = vec<int32, 1>;
using float32x8 = vec<float32, 8>;
using int32x8 = vec<int32, 8>;

//*****************************************************************************

template <>
inline float32x8 load<float32, 8>(const void *addr) {
  return _mm256_loadu_ps((float32 *)addr);
}

template <>
inline vec<int32, 8> load<int32, 8>(const void *addr) {
  return _mm256_loadu_si256((__m256i *)addr);
}

//*****************************************************************************
template <>
inline float32x8 gather<float32, 8>(const void *addr, int32x8 offsets) {
  // return _mm256_i32gather_ps((float32 *)addr, offsets, sizeof(float32));
  return _mm256_i32gather_ps((float32 *)addr, offsets, 4);
}

//*****************************************************************************

template <>
inline void store<float32, 8>(const float32x8 &v, const void *addr) {
  _mm256_storeu_ps((float32 *)addr, v);
}

template <>
inline void store<int32, 8>(const int32x8 &v, const void *addr) {
  _mm256_storeu_si256((__m256i *)addr, v);
}

//*****************************************************************************

template <>
inline void store<float32, 8>(const float32x8 &v, void *addr, int32x8 offsets) {
  // _mm256_i32scatter_ps(addr, offsets, v, sizeof(float32));
  for (int i = 0; i < 8; i++) {
    auto off = ((int *)&offsets)[i];
    ((float32 *)addr)[off] = v[i];
    // std::cout << off << "," << v[i] << std::endl;
  }
}

//*****************************************************************************

template <typename T, int dim>
inline vec<T, dim> floor(const vec<T, dim> &);

template <>
inline float32x8 floor<float32, 8>(const float32x8 &v) {
  return _mm256_floor_ps(v);
}

//*****************************************************************************

template <typename G, typename T, int dim>
inline vec<G, dim> cast(const vec<T, dim> &);

template <>
inline int32x8 cast<int32, float32, 8>(const float32x8 &v) {
  return _mm256_cvtps_epi32(v);
}

template <>
inline float32x8 cast<float32, int32, 8>(const int32x8 &v) {
  return _mm256_cvtepi32_ps(v);
}

//*****************************************************************************

template <>
inline float32x1 set1<float32, 1>(float32 v) {
  return v;
}

template <>
inline float32x8 set1<float32, 8>(float32 v) {
  return _mm256_set1_ps(v);
}

template <>
inline int32x1 set1<int32, 1>(int32 v) {
  return v;
}

template <>
inline int32x8 set1<int32, 8>(int32 v) {
  return _mm256_set1_epi32(v);
}

//*****************************************************************************

template <typename T, int dim>
inline vec<T, dim> min(vec<T, dim>, vec<T, dim>);

template <>
inline int32x8 min<int32, 8>(int32x8 a, int32x8 b) {
  return _mm256_min_epi32(a, b);
}

template <>
inline float32x8 min<float32, 8>(float32x8 a, float32x8 b) {
  return _mm256_min_ps(a, b);
}

template <typename T, int dim>
inline vec<T, dim> max(vec<T, dim>, vec<T, dim>);

template <>
inline int32x8 max<int32, 8>(int32x8 a, int32x8 b) {
  return _mm256_max_epi32(a, b);
}

template <>
inline float32x8 max<float32, 8>(float32x8 a, float32x8 b) {
  return _mm256_max_ps(a, b);
}

//*****************************************************************************
inline int32x8 cmp_ne(float32x8 a, float32x8 b) {
  auto ret = _mm256_cmp_ps(a, b, _CMP_NEQ_UQ);
  return *reinterpret_cast<int32x8 *>(&ret);
}

inline int32x8 cmp_ne(int32x8 a, int32x8 b) {
  auto ret = _mm256_cmp_ps(*reinterpret_cast<float32x8 *>(&a),
                           *reinterpret_cast<float32x8 *>(&b), _CMP_NEQ_UQ);
  return *reinterpret_cast<int32x8 *>(&ret);
}

inline int32x8 cmp_lt(float32x8 a, float32x8 b) {
  auto ret = _mm256_cmp_ps(a, b, _CMP_LT_OQ);
  return *reinterpret_cast<int32x8 *>(&ret);
}

inline int32x8 cmp_lt(int32x8 a, int32x8 b) {
  auto ret = _mm256_cmpgt_epi32(b, a);
  return ret;
}

//*****************************************************************************

inline float32x8 select(int32x8 mask, float32x8 true_val, float32x8 false_val) {
  return _mm256_blendv_ps(false_val, true_val,
                          *reinterpret_cast<float32x8 *>(&mask));
}

inline int32x8 select(int32x8 mask, int32x8 true_val, int32x8 false_val) {
  auto ret = _mm256_blendv_ps(*reinterpret_cast<float32x8 *>(&false_val),
                              *reinterpret_cast<float32x8 *>(&true_val),
                              *reinterpret_cast<float32x8 *>(&mask));
  return *reinterpret_cast<int32x8 *>(&ret);
}

//*****************************************************************************

template <int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
inline int32x8 shuffle8x32(int32x8 a) {
  return _mm256_permutevar8x32_epi32(
      a, _mm256_set_epi32(i7, i6, i5, i4, i3, i2, i1, i0));
};

template <int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
inline float32x8 shuffle8x32(float32x8 a) {
  return _mm256_permutevar8x32_ps(
      a, _mm256_set_epi32(i7, i6, i5, i4, i3, i2, i1, i0));
};

inline float32x8 blend(float32x8 a, float32x8 b, int imm) {
  return _mm256_blend_ps(a, b, imm);
}

inline int32x8 blend(int32x8 a, int32x8 b, int imm) {
  return _mm256_blend_epi32(a, b, imm);
}

#define DEFINE_BINARY_OP(T, OP, INST) \
  inline T OP(T a, T b) {             \
    return INST(a, b);                \
  }

DEFINE_BINARY_OP(float32x8, add, _mm256_add_ps);
DEFINE_BINARY_OP(float32x8, sub, _mm256_sub_ps);
DEFINE_BINARY_OP(float32x8, mul, _mm256_mul_ps);
DEFINE_BINARY_OP(float32x8, div, _mm256_div_ps);
DEFINE_BINARY_OP(float32x8, min, _mm256_min_ps);
DEFINE_BINARY_OP(float32x8, max, _mm256_max_ps);

DEFINE_BINARY_OP(int32x8, add, _mm256_add_epi32);
DEFINE_BINARY_OP(int32x8, sub, _mm256_sub_epi32);
DEFINE_BINARY_OP(int32x8, mul, _mm256_mullo_epi32);
DEFINE_BINARY_OP(int32x8, min, _mm256_min_epi32);
DEFINE_BINARY_OP(int32x8, max, _mm256_max_epi32);
DEFINE_BINARY_OP(int32x8, land, _mm256_and_si256);
DEFINE_BINARY_OP(int32x8, lor, _mm256_or_si256);

#define DEFINE_BINARY_OP_MID(T, OP, INST) \
  inline T OP(T a, T b) {                 \
    return a INST b;                      \
  }

DEFINE_BINARY_OP_MID(float32x1, add, +);
DEFINE_BINARY_OP_MID(int32x1, add, +);

inline int32x8 shr(int32x8 a, int b) {
  return _mm256_srli_epi32(a, b);
}

inline int32x8 shl(int32x8 a, int b) {
  return _mm256_slli_epi32(a, b);
}

inline int32x8 land(int32x8 a, int b) {
  int32x8 B = _mm256_set1_epi32(b);
  int32x8 v = _mm256_and_si256(a, B);
  return v;
}

template <int dim>
inline vec<int32, dim> div(vec<int32, dim> a, vec<int32, dim> b) {
  vec<int32, dim> ret;
  for (int i = 0; i < dim; i++) {
    ret[i] = a[i] / b[i];
  }
  return ret;
};

template <typename T, int dim>
inline vec<T, dim> mod(vec<T, dim> a, vec<T, dim> b) {
  static_assert(std::is_integral<T>::value, "");
  // return _mm256_and_si256(a, _mm256_set1_epi32(511));
  return sub(a, mul(div(a, b), b));
};

// *****************************************************************************
// these structures are used for maintaining metadata and sparsity.
// Their look_up function takes a merged index, but they don't know where do the
// bits come from.

template <typename child_type, int n_>
struct fixed {
  static constexpr int n = n_;
  child_type children[n];
  TC_FORCE_INLINE child_type *look_up(int i) {  // i is flattened index
    return &children[i];
  }

  TC_FORCE_INLINE int get_n() const {
    return n;
  }
};

template <typename child_type, int max_n_>
struct dynamic {
  static constexpr int max_n = max_n_;
  std::vector<child_type> children;
  TC_FORCE_INLINE child_type *look_up(int i) {  // i is flattened index
    return &children[i];
  }

  TC_FORCE_INLINE int get_n() const {
    return children.size();
  }
};
// *****************************************************************************

template <int max_n_>
struct indirect {
  static constexpr int max_n = max_n_;
  int children[max_n];
  std::atomic<int> n;

  indirect() : n(0) {
  }

  TC_FORCE_INLINE int get_n() {
    return n;
  }

  TC_FORCE_INLINE int *look_up(int i) {  // i is flattened index
    return &children[i];
  }

  TC_FORCE_INLINE void touch(int i) {
    children[n++] = i;
    printf("p=%p\n", &n);
    printf("n=%d, i=%d\n", (int)n, i);
  }

  TC_FORCE_INLINE void clear() {
    n.store(0);
  }
};
// *****************************************************************************

}  // namespace Tlang

}  // namespace taichi

