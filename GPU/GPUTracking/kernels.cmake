# Copyright 2019-2020 CERN and copyright holders of ALICE O2.
# See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
# All rights not expressly granted are reserved.
#
# This software is distributed under the terms of the GNU General Public
# License v3 (GPL Version 3), copied verbatim in the file "COPYING".
#
# In applying this license CERN does not waive the privileges and immunities
# granted to it by virtue of its status as an Intergovernmental Organization
# or submit itself to any jurisdiction.

# file kernels.cmake
# author David Rohr

o2_gpu_add_kernel("GPUTPCNeighboursFinder"                       "GPUTPCNeighboursFinder"          LB_OCL1 single)
o2_gpu_add_kernel("GPUTPCNeighboursCleaner"                      "GPUTPCNeighboursCleaner"         LB_OCL1 single)
o2_gpu_add_kernel("GPUTPCStartHitsFinder"                        "GPUTPCStartHitsFinder"           LB_OCL1 single)
o2_gpu_add_kernel("GPUTPCStartHitsSorter"                        "GPUTPCStartHitsSorter"           LB_OCL1 single)
o2_gpu_add_kernel("GPUTPCTrackletConstructor, singleSlice"       "GPUTPCTrackletConstructor"       LB_OCL1 single)
o2_gpu_add_kernel("GPUTPCTrackletConstructor, allSlices"         "GPUTPCTrackletConstructor"       LB_OCL1 single)
o2_gpu_add_kernel("GPUTPCTrackletSelector"                       "GPUTPCTrackletSelector"          LB_OCL1 both)
o2_gpu_add_kernel("GPUMemClean16"                                "GPUGeneralKernels"               NO_OCL1 "simple, REG, (GPUCA_THREAD_COUNT, 1)" void* ptr "unsigned long" size)
o2_gpu_add_kernel("GPUitoa"                                      "GPUGeneralKernels"               NO_OCL1 "simple, REG, (GPUCA_THREAD_COUNT, 1)" int* ptr "unsigned long" size)
o2_gpu_add_kernel("GPUTPCGlobalTrackingCopyNumbers"              "GPUTPCGlobalTracking"            NO_OCL1 single int n)
o2_gpu_add_kernel("GPUTPCCreateSliceData"                        "GPUTPCCreateSliceData"           LB      single)
o2_gpu_add_kernel("GPUTPCGlobalTracking"                         "GPUTPCGlobalTracking"            LB      single)
o2_gpu_add_kernel("GPUTPCSectorDebugSortKernels, hitData"        "GPUTPCSectorDebugSortKernels"    NO      single)
o2_gpu_add_kernel("GPUTPCSectorDebugSortKernels, startHits"      "GPUTPCSectorDebugSortKernels"    NO      single)
o2_gpu_add_kernel("GPUTPCSectorDebugSortKernels, sliceTracks"    "GPUTPCSectorDebugSortKernels"    NO      single)
o2_gpu_add_kernel("GPUTPCGlobalDebugSortKernels, clearIds"       "GPUTPCGlobalDebugSortKernels"    NO      single char parameter)
o2_gpu_add_kernel("GPUTPCGlobalDebugSortKernels, sectorTracks"   "GPUTPCGlobalDebugSortKernels"    NO      single char parameter)
o2_gpu_add_kernel("GPUTPCGlobalDebugSortKernels, globalTracks1"  "GPUTPCGlobalDebugSortKernels"    NO      single char parameter)
o2_gpu_add_kernel("GPUTPCGlobalDebugSortKernels, globalTracks2"  "GPUTPCGlobalDebugSortKernels"    NO      single char parameter)
o2_gpu_add_kernel("GPUTPCGlobalDebugSortKernels, borderTracks"   "GPUTPCGlobalDebugSortKernels"    NO      single char parameter)
o2_gpu_add_kernel("GPUTPCCreateOccupancyMap, fill"               "GPUTPCCreateOccupancyMap"        LB      simple GPUTPCClusterOccupancyMapBin* map)
o2_gpu_add_kernel("GPUTPCCreateOccupancyMap, fold"               "GPUTPCCreateOccupancyMap"        LB      simple GPUTPCClusterOccupancyMapBin* map)
o2_gpu_add_kernel("GPUTPCGMMergerTrackFit"                       "GPUTPCGMMergerGPU"               LB      simple int mode)
o2_gpu_add_kernel("GPUTPCGMMergerFollowLoopers"                  "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerUnpackResetIds"                 "GPUTPCGMMergerGPU"               LB      simple int iSlice)
o2_gpu_add_kernel("GPUTPCGMMergerSliceRefit"                     "GPUTPCGMMergerGPU"               LB      simple int iSlice)
o2_gpu_add_kernel("GPUTPCGMMergerUnpackGlobal"                   "GPUTPCGMMergerGPU"               LB      simple int iSlice)
o2_gpu_add_kernel("GPUTPCGMMergerUnpackSaveNumber"               "GPUTPCGMMergerGPU"               NO      simple int id)
o2_gpu_add_kernel("GPUTPCGMMergerResolve, step0"                 "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerResolve, step1"                 "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerResolve, step2"                 "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerResolve, step3"                 "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerResolve, step4"                 "GPUTPCGMMergerGPU"               LB      simple char useOrigTrackParam char mergeAll)
o2_gpu_add_kernel("GPUTPCGMMergerClearLinks"                     "GPUTPCGMMergerGPU"               LB      simple char output)
o2_gpu_add_kernel("GPUTPCGMMergerMergeWithinPrepare"             "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerMergeSlicesPrepare"             "GPUTPCGMMergerGPU"               LB      simple int border0 int border1 char useOrigTrackParam)
o2_gpu_add_kernel("GPUTPCGMMergerMergeBorders, step0"            "GPUTPCGMMergerGPU"               LB      simple int iSlice char withinSlice char mergeMode)
o2_gpu_add_kernel("GPUTPCGMMergerMergeBorders, step1"            "GPUTPCGMMergerGPU"               NO      simple int iSlice char withinSlice char mergeMode)
o2_gpu_add_kernel("GPUTPCGMMergerMergeBorders, step2"            "GPUTPCGMMergerGPU"               LB      simple int iSlice char withinSlice char mergeMode)
o2_gpu_add_kernel("GPUTPCGMMergerMergeBorders, variant"          "GPUTPCGMMergerGPU"               NO      simple gputpcgmmergertypes::GPUTPCGMBorderRange* range int N int cmpMax)
o2_gpu_add_kernel("GPUTPCGMMergerMergeCE"                        "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerLinkGlobalTracks"               "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerCollect"                        "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerSortTracks"                     "GPUTPCGMMergerGPU"               NO      simple)
o2_gpu_add_kernel("GPUTPCGMMergerSortTracksQPt"                  "GPUTPCGMMergerGPU"               NO      simple)
o2_gpu_add_kernel("GPUTPCGMMergerSortTracksPrepare"              "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerPrepareClusters, step0"         "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerPrepareClusters, step1"         "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerPrepareClusters, step2"         "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerFinalize, step0"                "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerFinalize, step1"                "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerFinalize, step2"                "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerMergeLoopers, step0"            "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerMergeLoopers, step1"            "GPUTPCGMMergerGPU"               LB      simple)
o2_gpu_add_kernel("GPUTPCGMMergerMergeLoopers, step2"            "GPUTPCGMMergerGPU"               LB      simple)

if(ALIGPU_BUILD_TYPE STREQUAL "O2" OR CONFIG_O2_EXTENSIONS)
o2_gpu_add_kernel("GPUTPCGMO2Output, prepare"                    "GPUTPCGMO2Output"                LB      simple)
o2_gpu_add_kernel("GPUTPCGMO2Output, sort"                       "GPUTPCGMO2Output"                NO      simple)
o2_gpu_add_kernel("GPUTPCGMO2Output, output"                     "GPUTPCGMO2Output"                LB      simple)
o2_gpu_add_kernel("GPUTPCGMO2Output, mc"                         "GPUTPCGMO2Output"                NO      simple)
o2_gpu_add_kernel("GPUTRDTrackerKernels, gpuVersion"             "GPUTRDTrackerKernels"            LB      simple GPUTRDTrackerGPU* externalInstance)
o2_gpu_add_kernel("GPUTRDTrackerKernels, o2Version"              "GPUTRDTrackerKernels"            LB      simple GPUTRDTracker* externalInstance)
o2_gpu_add_kernel("GPUITSFitterKernel"                           "GPUITSFitterKernels"             LB      simple)
o2_gpu_add_kernel("GPUTPCConvertKernel"                          "GPUTPCConvertKernel"             LB      simple)
o2_gpu_add_kernel("GPUTPCCompressionKernels, step0attached"      "GPUTPCCompressionKernels"        LB      simple)
o2_gpu_add_kernel("GPUTPCCompressionKernels, step1unattached"    "GPUTPCCompressionKernels"        LB      simple)
o2_gpu_add_kernel("GPUTPCCompressionGatherKernels, unbuffered"   "GPUTPCCompressionKernels"        LB      simple)
o2_gpu_add_kernel("GPUTPCCompressionGatherKernels, buffered32"   "GPUTPCCompressionKernels"        LB      simple)
o2_gpu_add_kernel("GPUTPCCompressionGatherKernels, buffered64"   "GPUTPCCompressionKernels"        LB      simple)
o2_gpu_add_kernel("GPUTPCCompressionGatherKernels, buffered128"  "GPUTPCCompressionKernels"        LB      simple)
o2_gpu_add_kernel("GPUTPCCompressionGatherKernels, multiBlock"   "GPUTPCCompressionKernels"        LB      simple)
o2_gpu_add_kernel("GPUTPCCFCheckPadBaseline"                     "GPUTPCClusterFinderKernels"      LB      single)
o2_gpu_add_kernel("GPUTPCCFChargeMapFiller, fillIndexMap"        "GPUTPCClusterFinderKernels"      LB      single)
o2_gpu_add_kernel("GPUTPCCFChargeMapFiller, fillFromDigits"      "GPUTPCClusterFinderKernels"      LB      single)
o2_gpu_add_kernel("GPUTPCCFChargeMapFiller, findFragmentStart"   "GPUTPCClusterFinderKernels"      LB      single char setPositions)
o2_gpu_add_kernel("GPUTPCCFPeakFinder"                           "GPUTPCClusterFinderKernels"      LB      single)
o2_gpu_add_kernel("GPUTPCCFNoiseSuppression, noiseSuppression"   "GPUTPCClusterFinderKernels"      LB      single)
o2_gpu_add_kernel("GPUTPCCFNoiseSuppression, updatePeaks"        "GPUTPCClusterFinderKernels"      LB      single)
o2_gpu_add_kernel("GPUTPCCFDeconvolution"                        "GPUTPCClusterFinderKernels"      LB      single)
o2_gpu_add_kernel("GPUTPCCFClusterizer"                          "GPUTPCClusterFinderKernels"      LB      single char onlyMC)
o2_gpu_add_kernel("GPUTPCCFMCLabelFlattener, setRowOffsets"      "GPUTPCClusterFinderKernels"      NO      single)
o2_gpu_add_kernel("GPUTPCCFMCLabelFlattener, flatten"            "GPUTPCClusterFinderKernels"      NO      single GPUTPCLinearLabels* out)
o2_gpu_add_kernel("GPUTPCCFStreamCompaction, scanStart"          "GPUTPCClusterFinderKernels"      LB      single int iBuf int stage)
o2_gpu_add_kernel("GPUTPCCFStreamCompaction, scanUp"             "GPUTPCClusterFinderKernels"      LB      single int iBuf int nElems)
o2_gpu_add_kernel("GPUTPCCFStreamCompaction, scanTop"            "GPUTPCClusterFinderKernels"      LB      single int iBuf int nElems)
o2_gpu_add_kernel("GPUTPCCFStreamCompaction, scanDown"           "GPUTPCClusterFinderKernels"      LB      single int iBuf "unsigned int" offset int nElems)
o2_gpu_add_kernel("GPUTPCCFStreamCompaction, compactDigits"      "GPUTPCClusterFinderKernels"      LB      single int iBuf int stage ChargePos* in ChargePos* out)
o2_gpu_add_kernel("GPUTPCCFDecodeZS"                             "GPUTPCClusterFinderKernels"      LB      single int firstHBF)
o2_gpu_add_kernel("GPUTPCCFDecodeZSLink"                         "GPUTPCClusterFinderKernels"      LB      single int firstHBF)
o2_gpu_add_kernel("GPUTPCCFDecodeZSDenseLink"                    "GPUTPCClusterFinderKernels"      LB      single int firstHBF)
o2_gpu_add_kernel("GPUTPCCFGather"                               "GPUTPCClusterFinderKernels"      LB      single o2::tpc::ClusterNative* dest)
o2_gpu_add_kernel("GPUTrackingRefitKernel, mode0asGPU"           "GPUTrackingRefitKernel"          LB      simple)
o2_gpu_add_kernel("GPUTrackingRefitKernel, mode1asTrackParCov"   "GPUTrackingRefitKernel"          LB      simple)
endif()
