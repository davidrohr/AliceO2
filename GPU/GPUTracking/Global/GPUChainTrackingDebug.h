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

/// \file GPUChainTrackingDEBUG.h
/// \author David Rohr

#ifndef GPUCHAINTRACKINGDEBUG_H
#define GPUCHAINTRACKINGDEBUG_H

#include <cstdint>
#include <functional>
#include <fstream>

namespace o2::gpu
{
// NOTE: Values below 262144 are activated by default with --debug 6 in GPUSettingsList.h::debugMask
enum GPUChainTrackingDebugFlags : uint32_t {
  TPCSectorTrackingData = 1,
  TPCPreLinks = 2,
  TPCLinks = 4,
  TPCStartHits = 8,
  TPCTracklets = 16,
  TPCSectorTracks = 32,
  TPCHitWeights = 256,
  TPCCompressedClusters = 512,
  TPCDecompressedClusters = 1024,
  TPCMergingRanges = 2048,
  TPCMergingSectorTracks = 4096,
  TPCMergingMergedTracks = 8192,
  TPCMergingCollectedTracks = 16384,
  TPCMergingCE = 32768,
  TPCMergingRefit = 65536,
  TPCClustererClusters = 131072,
  TPCClusterer = 262144,
  TPCClustererDigits = 262144 << 1,
  TPCClustererPeaks = 262144 << 2,
  TPCClustererSuppressedPeaks = 262144 << 3,
  TPCClustererChargeMap = 262144 << 4,
  TPCClustererZeroedCharges = 262144 << 5
};

template <class T, class S, typename... Args>
inline bool GPUChain::DoDebugAndDump(GPUChain::RecoStep step, uint32_t mask, bool transfer, T& processor, S T::*func, Args&&... args)
{
  if (GetProcessingSettings().keepAllMemory) {
    if (transfer) {
      TransferMemoryResourcesToHost(step, &processor, -1, true);
    }
    std::function<void(Args && ...)> lambda = [&processor, &func](Args&... args_tmp) {
      if (func) {
        (processor.*func)(args_tmp...);
      }
    };
    return DoDebugDump(mask, lambda, args...);
  }
  return false;
}

template <typename... Args>
inline bool GPUChain::DoDebugDump(uint32_t mask, std::function<void(Args&...)> func, Args&... args)
{
  if (GetProcessingSettings().debugLevel >= 6 && (mask == 0 || (GetProcessingSettings().debugMask & mask))) {
    func(args...);
    return true;
  }
  return false;
}

} // namespace o2::gpu

#endif
