// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "GPUWorkflowHelper/GPUWorkflowHelper.h"
#include "ITStracking/IOUtils.h"
using namespace o2::globaltracking;
using namespace o2::gpu;

struct GPUWorkflowHelper::tmpDataContainer {
  std::vector<o2::BaseCluster<float>> ITSClustersArray;
};

std::unique_ptr<const GPUWorkflowHelper::tmpDataContainer> GPUWorkflowHelper::fillIOPtr(GPUTrackingInOutPointers& ioPtr, const o2::globaltracking::RecoContainer& recoCont, const GPUCalibObjectsConst* calib, o2::dataformats::GlobalTrackID::mask_t maskCl, o2::dataformats::GlobalTrackID::mask_t maskTrk)
{
  auto retVal = std::make_unique<tmpDataContainer>();

  const auto& ITSClusterROFRec = recoCont.getITSClustersROFRecords<o2::itsmft::ROFRecord>();
  const auto& clusITS = recoCont.getITSClusters<o2::itsmft::CompClusterExt>();
  if (calib && calib->itsPatternDict) {
    const auto& patterns = recoCont.getITSClustersPatterns();
    auto pattIt = patterns.begin();
    retVal->ITSClustersArray.reserve(clusITS.size());
    o2::its::ioutils::convertCompactClusters(clusITS, pattIt, retVal->ITSClustersArray, *calib->itsPatternDict);
  }
  const auto& ITSTrkLabels = recoCont.getITSTracksMCLabels();
  const auto& ITSClsLabels = recoCont.mcITSClusters.get();

  const auto& ITSTracksArray = recoCont.getITSTracks<o2::its::TrackITS>();
  const auto& ITSTrackClusIdx = recoCont.getITSTracksClusterRefs();
  const auto& ITSTrackROFRec = recoCont.getITSTracksROFRecords<o2::itsmft::ROFRecord>();
  const auto& mITSTrkLabels = recoCont.getITSTracksMCLabels();

  ioPtr.nItsClusters = clusITS.size();
  ioPtr.itsCompClusters = clusITS.data();
  ioPtr.nItsClusterROF = ITSClusterROFRec.size();
  ioPtr.itsClusterROF = ITSClusterROFRec.data();
  ioPtr.itsClusterMC = ITSClsLabels;
  ioPtr.itsClusters = retVal->ITSClustersArray.data();
  ioPtr.nItsTracks = ITSTracksArray.size();
  ioPtr.itsTracks = ITSTracksArray.data();
  ioPtr.itsTrackClusIdx = ITSTrackClusIdx.data();
  ioPtr.nItsTrackROF = ITSTrackROFRec.size();
  ioPtr.itsTrackROF = ITSTrackROFRec.data();
  ioPtr.itsTrackMC = ITSTrkLabels.data();

  return std::move(retVal);
}
