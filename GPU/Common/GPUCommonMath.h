// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUCommonMath.h
/// \author David Rohr, Sergey Gorbunov

#ifndef GPUCOMMONMATH_H
#define GPUCOMMONMATH_H

#include "GPUCommonDef.h"

#if !defined(__OPENCL__)
#include <cmath>
#include <algorithm>
#endif

#if !defined(__OPENCL__) || defined(__OPENCLCPP__)
namespace GPUCA_NAMESPACE
{
namespace gpu
{
#endif

class GPUCommonMath
{
 public:
  GPUhdni() static float2 MakeFloat2(float x, float y);

  template <class T>
  GPUhd() static T Min(T x, T y);
  template <class T>
  GPUhd() static T Max(T x, T y);
  GPUhdni() static float Sqrt(float x);
  template <class T>
  GPUhd() static T Abs(T x);
  GPUhdni() static float ASin(float x);
  GPUhdni() static float ATan(float x);
  GPUhdni() static float ATan2(float y, float x);
  GPUhdni() static float Sin(float x);
  GPUhdni() static float Cos(float x);
  GPUhdni() static float Tan(float x);
  GPUhdni() static float Copysign(float x, float y);
  GPUhdni() static float TwoPi() { return 6.28319f; }
  GPUhdni() static float Pi() { return 3.1415926535897f; }
  GPUhdni() static int Nint(float x);
  GPUhdni() static bool Finite(float x);

  GPUhdni() static float Log(float x);
  GPUd() static int AtomicExch(GPUglobalref() GPUAtomic(int) * addr, int val);
  GPUd() static int AtomicAdd(GPUglobalref() GPUAtomic(int) * addr, int val);
  GPUd() static void AtomicMax(GPUglobalref() GPUAtomic(int) * addr, int val);
  GPUd() static void AtomicMin(GPUglobalref() GPUAtomic(int) * addr, int val);
  GPUd() static int AtomicExchShared(GPUsharedref() GPUAtomic(int) * addr, int val);
  GPUd() static int AtomicAddShared(GPUsharedref() GPUAtomic(int) * addr, int val);
  GPUd() static void AtomicMaxShared(GPUsharedref() GPUAtomic(int) * addr, int val);
  GPUd() static void AtomicMinShared(GPUsharedref() GPUAtomic(int) * addr, int val);
  GPUd() static int Mul24(int a, int b);
  GPUd() static float FMulRZ(float a, float b);
};

typedef GPUCommonMath CAMath;

#if defined(GPUCA_GPUCODE_DEVICE) && (defined(__CUDACC__) || defined(__HIPCC_)) // clang-format off
    #define CHOICE(c1, c2, c3) c2
#elif defined(GPUCA_GPUCODE_DEVICE) && defined (__OPENCL__)
    #define CHOICE(c1, c2, c3) c3
#else
    #define CHOICE(c1, c2, c3) c1
#endif // clang-format on

GPUhdi() float2 GPUCommonMath::MakeFloat2(float x, float y)
{
#if !defined(GPUCA_GPUCODE) || defined(__OPENCL__)
  float2 ret = { x, y };
  return ret;
#else
  return make_float2(x, y);
#endif // GPUCA_GPUCODE
}

GPUhdi() int GPUCommonMath::Nint(float x)
{
  int i;
  if (x >= 0) {
    i = int(x + 0.5f);
    if (x + 0.5f == float(i) && i & 1)
      i--;
  } else {
    i = int(x - 0.5f);
    if (x - 0.5f == float(i) && i & 1)
      i++;
  }
  return i;
}

GPUhdi() bool GPUCommonMath::Finite(float x) { return CHOICE(std::isfinite(x), true, true); }

GPUhdi() float GPUCommonMath::ATan(float x) { return CHOICE(atanf(x), atanf(x), atan(x)); }

GPUhdi() float GPUCommonMath::ATan2(float y, float x) { return CHOICE(atan2f(y, x), atan2f(y, x), atan2(y, x)); }

GPUhdi() float GPUCommonMath::Sin(float x) { return CHOICE(sinf(x), sinf(x), sin(x)); }

GPUhdi() float GPUCommonMath::Cos(float x) { return CHOICE(cosf(x), cosf(x), cos(x)); }

GPUhdi() float GPUCommonMath::Tan(float x) { return CHOICE(tanf(x), tanf(x), tan(x)); }

template <class T>
GPUhdi() T GPUCommonMath::Min(T x, T y)
{
  return CHOICE(std::min(x, y), std::min(x, y), (x < y ? x : y));
}

template <class T>
GPUhdi() T GPUCommonMath::Max(T x, T y)
{
  return CHOICE(std::max(x, y), std::max(x, y), (x > y ? x : y));
}

GPUhdi() float GPUCommonMath::Sqrt(float x) { return CHOICE(sqrtf(x), sqrtf(x), sqrt(x)); }

template <>
GPUhdi() float GPUCommonMath::Abs<float>(float x)
{
  return CHOICE(fabsf(x), fabsf(x), fabs(x));
}

#if !defined(__OPENCL__) || defined(cl_khr_fp64)
template <>
GPUhdi() double GPUCommonMath::Abs<double>(double x)
{
  return CHOICE(fabs(x), fabs(x), fabs(x));
}
#endif

template <>
GPUhdi() int GPUCommonMath::Abs<int>(int x)
{
  return CHOICE(abs(x), abs(x), abs(x));
}

GPUhdi() float GPUCommonMath::ASin(float x) { return CHOICE(asinf(x), asinf(x), asin(x)); }

GPUhdi() float GPUCommonMath::Log(float x) { return CHOICE(logf(x), logf(x), log(x)); }

GPUhdi() float GPUCommonMath::Copysign(float x, float y)
{
#if defined(__OPENCLCPP__)
  return copysign(x, y);
#elif defined(GPUCA_GPUCODE) && !defined(__OPENCL__)
  return copysignf(x, y);
#elif defined(__cplusplus) && __cplusplus >= 201103L
  return std::copysignf(x, y);
#else
  x = GPUCommonMath::Abs(x);
  return (y >= 0) ? x : -x;
#endif // GPUCA_GPUCODE
}

#if defined(__OPENCL__) && !defined(__OPENCLCPP__)
GPUdi() int GPUCommonMath::AtomicExchShared(GPUsharedref() GPUAtomic(int) * addr, int val)
{
  return ::atomic_xchg((volatile __local int*)addr, val);
}
GPUdi() int GPUCommonMath::AtomicAddShared(GPUsharedref() GPUAtomic(int) * addr, int val) { return ::atomic_add((volatile __local int*)addr, val); }
GPUdi() void GPUCommonMath::AtomicMaxShared(GPUsharedref() GPUAtomic(int) * addr, int val) { ::atomic_max((volatile __local int*)addr, val); }
GPUdi() void GPUCommonMath::AtomicMinShared(GPUsharedref() GPUAtomic(int) * addr, int val) { ::atomic_min((volatile __local int*)addr, val); }
#else
GPUdi() int GPUCommonMath::AtomicExchShared(GPUAtomic(int) * addr, int val)
{
  return GPUCommonMath::AtomicExch(addr, val);
}
GPUdi() int GPUCommonMath::AtomicAddShared(GPUAtomic(int) * addr, int val) { return GPUCommonMath::AtomicAdd(addr, val); }
GPUdi() void GPUCommonMath::AtomicMaxShared(GPUAtomic(int) * addr, int val) { GPUCommonMath::AtomicMax(addr, val); }
GPUdi() void GPUCommonMath::AtomicMinShared(GPUAtomic(int) * addr, int val) { GPUCommonMath::AtomicMin(addr, val); }
#endif

#ifndef GPUCA_GPUCODE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value" // GCC BUG in omp atomic capture gives false warning
#endif

GPUdi() int GPUCommonMath::AtomicExch(GPUglobalref() GPUAtomic(int) * addr, int val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCLCPP__) && !defined(__clang__)
  return atomic_exchange(addr, val);
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  return ::atomic_xchg((volatile __global int*)addr, val);
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC_))
  return ::atomicExch(addr, val);
#else
  int old;
#ifdef GPUCA_HAVE_OPENMP
#pragma omp atomic capture
#endif
  {
    old = *addr;
    *addr = val;
  }
  return old;
#endif // GPUCA_GPUCODE
}

GPUdi() int GPUCommonMath::AtomicAdd(GPUglobalref() GPUAtomic(int) * addr, int val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCLCPP__) && !defined(__clang__)
  return atomic_fetch_add(addr, val);
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  return ::atomic_add((volatile __global int*)addr, val);
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC_))
  return ::atomicAdd(addr, val);
#else
  int old;
#ifdef GPUCA_HAVE_OPENMP
#pragma omp atomic capture
#endif
  {
    old = *addr;
    *addr += val;
  }
  return old;
#endif // GPUCA_GPUCODE
}

GPUdi() void GPUCommonMath::AtomicMax(GPUglobalref() GPUAtomic(int) * addr, int val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCLCPP__) && !defined(__clang__)
  atomic_fetch_max(addr, val);
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  ::atomic_max((volatile __global int*)addr, val);
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC_))
  ::atomicMax(addr, val);
#else
#ifdef GPUCA_HAVE_OPENMP
  while (*addr < val)
    AtomicExch(addr, val);
#else
  if (*addr < val)
    *addr = val;
#endif
#endif // GPUCA_GPUCODE
}

GPUdi() void GPUCommonMath::AtomicMin(GPUglobalref() GPUAtomic(int) * addr, int val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCLCPP__) && !defined(__clang__)
  atomic_fetch_min(addr, val);
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  ::atomic_min((volatile __global int*)addr, val);
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC_))
  ::atomicMin(addr, val);
#else
#ifdef GPUCA_HAVE_OPENMP
  while (*addr > val)
    AtomicExch(addr, val);
#else
  if (*addr > val)
    *addr = val;
#endif
#endif // GPUCA_GPUCODE
}

#ifndef GPUCA_GPUCODE
#pragma GCC diagnostic pop
#endif

#undef CHOICE

#if !defined(__OPENCL__) || defined(__OPENCLCPP__)
}
}
#endif

#endif // GPUCOMMONMATH_H
