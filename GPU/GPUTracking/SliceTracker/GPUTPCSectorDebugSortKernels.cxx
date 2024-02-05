// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUTPCSectorDebugSortKernels.cxx
/// \author David Rohr

#include "GPUParam.h"
#include "GPUTPCClusterData.h"
#include "GPUTPCHit.h"
#include "GPUTPCSliceData.h"
#include "GPUProcessor.h"
#include "GPUO2DataTypes.h"
#include "GPUCommonMath.h"
#include "GPUTPCSectorDebugSortKernels.h"

using namespace GPUCA_NAMESPACE::gpu;

template <>
GPUdii() void GPUTPCSectorDebugSortKernels::Thread<0>(int nBlocks, int nThreads, int iBlock, int iThread, GPUsharedref() GPUSharedMemory& smem, processorType& GPUrestrict() tracker)
{
}
