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

/// \file GPUTPCDecompressionKernels.h
/// \author Gabriele Cimador

#ifndef GPUTPCDECOMPRESSIONKERNELS_H
#define GPUTPCDECOMPRESSIONKERNELS_H

#include "GPUGeneralKernels.h"
#include "GPUO2DataTypes.h"
#include "GPUParam.h"
#include "GPUConstantMem.h"

#ifdef GPUCA_HAVE_O2HEADERS
#include "DataFormatsTPC/CompressedClusters.h"
#else
namespace o2::tpc
{
struct CompressedClusters {
};
} // namespace o2::tpc
#endif

namespace GPUCA_NAMESPACE::gpu
{

class GPUTPCDecompressionKernels : public GPUKernelTemplate
{
 public:
  GPUhdi() constexpr static GPUDataTypes::RecoStep GetRecoStep() { return GPUDataTypes::RecoStep::TPCDecompression; }

  enum K : int {
    step0attached = 0,
    step1unattached = 1,
    prepareAccess = 2
  };

  template <int iKernel = defaultKernel>
  GPUd() static void Thread(int nBlocks, int nThreads, int iBlock, int iThread, GPUsharedref() GPUSharedMemory& smem, processorType& GPUrestrict() processors);
  GPUd() static void decompressTrack(o2::tpc::CompressedClusters& cmprClusters, const GPUParam& param, const unsigned int maxTime, const unsigned int trackIndex, unsigned int& clusterOffset, GPUTPCDecompression& decompressor);
  GPUdi() static o2::tpc::ClusterNative decompressTrackStore(const o2::tpc::CompressedClusters& cmprClusters, const unsigned int clusterOffset, unsigned int slice, unsigned int row, unsigned int pad, unsigned int time, GPUTPCDecompression& decompressor, bool& stored);

  GPUd() static unsigned int computeLinearTmpBufferIndex(unsigned int slice, unsigned int row, unsigned int maxClustersPerBuffer){
    return slice * (GPUCA_ROW_COUNT * maxClustersPerBuffer) + row * maxClustersPerBuffer;
  }
};

}
#endif // GPUTPCDECOMPRESSIONKERNELS_H
