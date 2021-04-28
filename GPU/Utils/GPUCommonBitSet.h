// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUCommonBitSet.h
/// \author David Rohr

#ifndef GPUCOMMONBITSET_H
#define GPUCOMMONBITSET_H

// Limited reimplementation of std::bitset for the GPU.
// Fixed to 32 bits for now.
// In contrast to the GPUCommonArray, we cannot just use std::bitset on the host.
// The layout may be implementation defined, so it is not guarantueed that we
// get correct data after copying it into a gpustd::bitset on the GPU.

#include "GPUCommonDef.h"
#include "GPUCommonRtypes.h"
namespace o2::gpu::gpustd
{
template <unsigned int N>
class bitset
{
  static_assert(N <= 32, "> 32 bits not supported");

 public:
  GPUdDefault() bitset() = default;
  GPUd() bitset(unsigned int vv) : v(vv){};
  static constexpr unsigned int full_set = ((1u << N) - 1);

  GPUd() bool all() const { return (v & full_set) == full_set; }
  GPUd() bool any() const { return v & full_set; }
  GPUd() bool none() const { return !any(); }

  GPUd() void set() { v = full_set; }
  GPUd() void set(unsigned int i) { v |= (1u << i) & full_set; }
  GPUd() void reset() { v = 0; }
  GPUd() void reset(unsigned int i) { v &= ~(1u << i); }
  GPUd() void flip() { v = (~v) & full_set; }

  GPUd() bitset& operator=(const bitset&) = default;
  GPUd() bitset operator|(const bitset b) const { return v | b.v; }
  GPUd() bitset& operator|=(const bitset b)
  {
    b |= b.v;
    return *this;
  }
  GPUd() bitset operator&(const bitset b) const { return v & b.v; }
  GPUd() bitset& operator&=(const bitset b)
  {
    v &= b.v;
    return *this;
  }
  GPUd() bitset operator~() const { return (~v) & full_set; }
  GPUd() bool operator==(const bitset b) { return v == b.v; }
  GPUd() bool operator!=(const bitset b) { return v != b.v; }

  GPUd() bool operator[](unsigned int i) { return (v >> i) & 1u; }

  GPUd() unsigned int to_ulong() const { return v; }

 private:
  unsigned int v = 0;

  ClassDefNV(bitset, 1);
};
} // namespace o2::gpu::gpustd

#endif
