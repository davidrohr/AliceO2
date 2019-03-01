// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUTPCDef.h
/// \author David Rohr, Sergey Gorbunov

// clang-format off
#ifndef GPUTPCDEF_H
#define GPUTPCDEF_H

/**
 * Definitions needed for GPUTPCTracker
 *
 */

#include "GPUCommonDef.h"
#include "GPUTPCSettings.h"
#include "GPUCommonRtypes.h"

//Macros for GRID dimension
#if defined(__CUDACC__)
  #define get_global_id(dim) (blockIdx.x * blockDim.x + threadIdx.x)
  #define get_global_size(dim) (blockDim.x * gridDim.x)
  #define get_num_groups(dim) (gridDim.x)
  #define get_local_id(dim) (threadIdx.x)
  #define get_local_size(dim) (blockDim.x)
  #define get_group_id(dim) (blockIdx.x)
#elif defined(__HIPCC__)
  #define get_global_id(dim) (hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x)
  #define get_global_size(dim) (hipBlockDim_x * hipGridDim_x)
  #define get_num_groups(dim) (hipGridDim_x)
  #define get_local_id(dim) (hipThreadIdx_x)
  #define get_local_size(dim) (hipBlockDim_x)
  #define get_group_id(dim) (hipBlockIdx_x)
#elif defined(__OPENCL__)
  //Using OpenCL defaults
#else
  #define get_global_id(dim) iBlock
  #define get_global_size(dim) nBlocks
  #define get_num_groups(dim) nBlocks
  #define get_local_id(dim) 0
  #define get_local_size(dim) 1
  #define get_group_id(dim) iBlock
#endif

//Special macros for OpenCL rev. 1.2 (encode address space in template parameter)
enum LocalOrGlobal { Mem_Local, Mem_Global, Mem_Constant, Mem_Plain };
#if defined(__OPENCL__) && !defined(__OPENCLCPP__)
  template<LocalOrGlobal, typename L, typename G, typename C, typename P> struct MakeTypeHelper;
  template<typename L, typename G, typename C, typename P> struct MakeTypeHelper<Mem_Local, L, G, C, P> { typedef L type; };
  template<typename L, typename G, typename C, typename P> struct MakeTypeHelper<Mem_Global, L, G, C, P> { typedef G type; };
  template<typename L, typename G, typename C, typename P> struct MakeTypeHelper<Mem_Constant, L, G, C, P> { typedef C type; };
  template<typename L, typename G, typename C, typename P> struct MakeTypeHelper<Mem_Plain, L, G, C, P> { typedef P type; };
  #define MakeType(base_type) typename MakeTypeHelper<LG, GPUshared() base_type, GPUglobalref() base_type, GPUconstant() base_type, base_type>::type
  #define MEM_CLASS_PRE() template<LocalOrGlobal LG>
  #define MEM_LG(type) type<LG>
  #define MEM_CLASS_PRE2() template<LocalOrGlobal LG2>
  #define MEM_LG2(type) type<LG2>
  #define MEM_CLASS_PRE12() template<LocalOrGlobal LG> template<LocalOrGlobal LG2>
  #define MEM_CLASS_PRE23() template<LocalOrGlobal LG2, LocalOrGlobal LG3>
  #define MEM_LG3(type) type<LG3>
  #define MEM_CLASS_PRE234() template<LocalOrGlobal LG2, LocalOrGlobal LG3, LocalOrGlobal LG4>
  #define MEM_LG4(type) type<LG4>
  #define MEM_GLOBAL(type) type<Mem_Global>
  #define MEM_LOCAL(type) type<Mem_Local>
  #define MEM_CONSTANT(type) type<Mem_Global>
  #define MEM_PLAIN(type) type<Mem_Plain>
  #define MEM_TEMPLATE() template <typename T>
  #define MEM_TYPE(type) T
  #define MEM_TEMPLATE2() template <typename T, typename T2>
  #define MEM_TYPE2(type) T2
  #define MEM_TEMPLATE3() template <typename T, typename T2, typename T3>
  #define MEM_TYPE3(type) T3
  #define MEM_TEMPLATE4() template <typename T, typename T2, typename T3, typename T4>
  #define MEM_TYPE4(type) T4
  //#define MEM_CONSTANT() <Mem_Constant> //Use __global for time being instead of __constant, see above
#else
  #define MakeType(base_type) base_type
  #define MEM_CLASS_PRE()
  #define MEM_LG(type) type
  #define MEM_CLASS_PRE2()
  #define MEM_LG2(type) type
  #define MEM_CLASS_PRE12()
  #define MEM_CLASS_PRE23()
  #define MEM_LG3(type) type
  #define MEM_CLASS_PRE234()
  #define MEM_LG4(type) type
  #define MEM_GLOBAL(type) type
  #define MEM_LOCAL(type) type
  #define MEM_CONSTANT(type) type
  #define MEM_PLAIN(type) type
  #define MEM_TEMPLATE()
  #define MEM_TYPE(type) type
  #define MEM_TEMPLATE2()
  #define MEM_TYPE2(type) type
  #define MEM_TEMPLATE3()
  #define MEM_TYPE3(type) type
  #define MEM_TEMPLATE4()
  #define MEM_TYPE4(type) type
#endif

#define CALINK_INVAL ((calink) -1)
struct cahit2{cahit x, y;};

#ifdef GPUCA_FULL_CLUSTERDATA
  #define GPUCA_EVDUMP_FILE "event_full"
#else
  #define GPUCA_EVDUMP_FILE "event"
#endif

#ifdef GPUseStatError
  #define GMPropagatePadRowTime
#endif

#ifdef GMPropagatePadRowTime //Needs full clusterdata
  #define GPUCA_FULL_CLUSTERDATA
#endif

#if defined(GPUCA_STANDALONE) || defined(GPUCA_GPUCODE) //No support for Full Field Propagator or Statistical errors
  #ifdef GMPropagatorUseFullField
    #undef GMPropagatorUseFullField
  #endif
  #ifdef GPUseStatError
    #undef GPUseStatError
  #endif
#endif

#ifdef EXTERN_ROW_HITS
  #define GETRowHit(iRow) tracker.TrackletRowHits()[iRow * s.mNTracklets + r.mItr]
  #define SETRowHit(iRow, val) tracker.TrackletRowHits()[iRow * s.mNTracklets + r.mItr] = val
#else
  #define GETRowHit(iRow) tracklet.RowHit(iRow)
  #define SETRowHit(iRow, val) tracklet.SetRowHit(iRow, val)
#endif

#ifdef GPUCA_GPUCODE
  #define MAKESharedRef(vartype, varname, varglobal, varshared) const GPUsharedref() MEM_LOCAL(vartype) &varname = varshared;
#else
  #define MAKESharedRef(vartype, varname, varglobal, varshared) const GPUglobalref() MEM_GLOBAL(vartype) &varname = varglobal;
#endif

#ifdef GPUCA_TEXTURE_FETCH_CONSTRUCTOR
  #define TEXTUREFetchCons(type, texture, address, entry) tex1Dfetch(texture, ((char*) address - tracker.Data().GPUTextureBase()) / sizeof(type) + entry);
#else
  #define TEXTUREFetchCons(type, texture, address, entry) address[entry];
#endif

#endif //GPUTPCDEF_H

#ifdef GPUCA_CADEBUG
  #ifdef CADEBUG
    #undef CADEBUG
  #endif
  #ifdef GPUCA_CADEBUG_ENABLED
    #undef GPUCA_CADEBUG_ENABLED
  #endif
  #if GPUCA_CADEBUG == 1 && !defined(GPUCA_GPUCODE)
    #define CADEBUG(...) __VA_ARGS__
    #define CADEBUG2(cmd, ...) {__VA_ARGS__; cmd;}
    #define GPUCA_CADEBUG_ENABLED
  #endif
  #undef GPUCA_CADEBUG
#endif

#ifndef CADEBUG
  #define CADEBUG(...)
  #define CADEBUG2(cmd, ...) {cmd;}
#endif
// clang-format on
