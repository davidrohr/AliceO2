// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUTrackingRefit.cxx
/// \author David Rohr

#include "GPUTrackingRefit.h"
#include "GPUO2DataTypes.h"
#include "GPUTPCGMTrackParam.h"
#include "GPUTPCGMMergedTrack.h"
#include "GPUTPCGMPropagator.h"
#include "GPUConstantMem.h"
#include "TPCFastTransform.h"
#include "ReconstructionDataFormats/Track.h"
#include "DetectorsBase/Propagator.h"
#include "DataFormatsTPC/TrackTPC.h"

using namespace GPUCA_NAMESPACE::gpu;
using namespace o2::track;
using namespace o2::base;
using namespace o2::tpc;

void GPUTrackingRefitProcessor::InitializeProcessor() {}

void GPUTrackingRefitProcessor::RegisterMemoryAllocation()
{
  AllocateAndInitializeLate();
}

void GPUTrackingRefitProcessor::SetMaxData(const GPUTrackingInOutPointers& io)
{
}

namespace
{
template <class T>
struct refitTrackTypes;
template <>
struct refitTrackTypes<GPUTPCGMTrackParam> {
  using propagator = GPUTPCGMPropagator;
};
template <>
struct refitTrackTypes<TrackParCov> {
  using propagator = const Propagator*;
};
} // anonymous namespace

template <>
void GPUTrackingRefit::initProp<GPUTPCGMPropagator>(GPUTPCGMPropagator& prop)
{
  prop.SetMaterialTPC();
  prop.SetMaxSinPhi(GPUCA_MAX_SIN_PHI);
  prop.SetToyMCEventsFlag(false);
  prop.SetSeedingErrors(false);
  prop.SetFitInProjections(false);
  prop.SetPropagateBzOnly(false);
  prop.SetPolynomialField(&mPparam->polynomialField);
}

template <>
void GPUTrackingRefit::initProp<const Propagator*>(const Propagator*& prop)
{
  prop = mPpropagator;
}

template <class T, class S, class U>
void GPUTrackingRefit::convertTrack(T& trk, const S& trkX, U& prop)
{
  trk = trkX;
}
static void convertTrackParam(GPUTPCGMTrackParam& trk, const TrackParCov& trkX)
{
  for (int i = 0; i < 15; i++) {
    trk.Par()[i] = trkX.getParams()[i];
  }
  for (int i = 0; i < 15; i++) {
    trk.Cov()[i] = trkX.getCov()[i];
  }
  trk.X() = trkX.getX();
}
static void convertTrackParam(TrackParCov& trk, const GPUTPCGMTrackParam& trkX)
{
  for (int i = 0; i < 5; i++) {
    trk.setParam(i, trkX.GetPar()[i]);
  }
  for (int i = 0; i < 15; i++) {
    trk.setCov(i, trkX.GetCov()[i]);
  }
  trk.setX(trkX.GetX());
}
template <>
void GPUTrackingRefit::convertTrack<GPUTPCGMMergedTrack, TrackParCov, const Propagator*>(GPUTPCGMMergedTrack& trk, const TrackParCov& trkX, const Propagator*& prop)
{
  convertTrackParam(trk.Param(), trkX);
  trk.SetAlpha(trkX.getAlpha());
}
template <>
void GPUTrackingRefit::convertTrack<TrackParCov, GPUTPCGMMergedTrack, const Propagator*>(TrackParCov& trk, const GPUTPCGMMergedTrack& trkX, const Propagator*& prop)
{
  convertTrackParam(trk, trkX.GetParam());
  trk.setAlpha(trkX.GetAlpha());
}
template <>
void GPUTrackingRefit::convertTrack<GPUTPCGMTrackParam, TrackParCov, GPUTPCGMPropagator>(GPUTPCGMTrackParam& trk, const TrackParCov& trkX, GPUTPCGMPropagator& prop)
{
  convertTrackParam(trk, trkX);
  initProp(prop);
  prop.SetTrack(&trk, trkX.getAlpha());
}
template <>
void GPUTrackingRefit::convertTrack<TrackParCov, GPUTPCGMTrackParam, GPUTPCGMPropagator>(TrackParCov& trk, const GPUTPCGMTrackParam& trkX, GPUTPCGMPropagator& prop)
{
  convertTrackParam(trk, trkX);
  trk.setAlpha(prop.GetAlpha());
}
template <>
void GPUTrackingRefit::convertTrack<GPUTPCGMTrackParam, GPUTPCGMMergedTrack, GPUTPCGMPropagator>(GPUTPCGMTrackParam& trk, const GPUTPCGMMergedTrack& trkX, GPUTPCGMPropagator& prop)
{
  trk = trkX.GetParam();
  initProp(prop);
  prop.SetTrack(&trk, trkX.GetAlpha());
}
template <>
void GPUTrackingRefit::convertTrack<GPUTPCGMMergedTrack, GPUTPCGMTrackParam, GPUTPCGMPropagator>(GPUTPCGMMergedTrack& trk, const GPUTPCGMTrackParam& trkX, GPUTPCGMPropagator& prop)
{
  trk.SetParam(trkX);
  trk.SetAlpha(prop.GetAlpha());
}
template <>
void GPUTrackingRefit::convertTrack<GPUTPCGMTrackParam, TrackTPC, GPUTPCGMPropagator>(GPUTPCGMTrackParam& trk, const TrackTPC& trkX, GPUTPCGMPropagator& prop)
{
  convertTrack<GPUTPCGMTrackParam, TrackParCov, GPUTPCGMPropagator>(trk, trkX, prop);
}
template <>
void GPUTrackingRefit::convertTrack<TrackTPC, GPUTPCGMTrackParam, GPUTPCGMPropagator>(TrackTPC& trk, const GPUTPCGMTrackParam& trkX, GPUTPCGMPropagator& prop)
{
  convertTrack<TrackParCov, GPUTPCGMTrackParam, GPUTPCGMPropagator>(trk, trkX, prop);
}
template <>
void GPUTrackingRefit::convertTrack<TrackTPC, TrackParCov, const Propagator*>(TrackTPC& trk, const TrackParCov& trkX, const Propagator*& prop)
{
  convertTrack<TrackParCov, TrackParCov, const Propagator*>(trk, trkX, prop);
}

template <class T, class S>
GPUd() int GPUTrackingRefit::RefitTrack(T& trkX, bool outward)
{
  typename refitTrackTypes<S>::propagator prop;
  S trk;
  convertTrack(trk, trkX, prop);
  int count;
  if constexpr (std::is_same<T, GPUTPCGMMergedTrack>::value) {
    count = trkX.NClusters();
  } else if constexpr (std::is_same<T, TrackTPC>::value) {
    count = trkX.getNClusters();
  }
  float zOffset = 0;
  for (int i = 0;i < count;i++) {
    const ClusterNative* cl;
    uint8_t sector;
    uint8_t row;
    if constexpr (std::is_same<T, GPUTPCGMMergedTrack>::value) {
      const auto& hit = mPtrackHits[trkX.FirstClusterRef() + i];
      cl = &mPclusterNative->clustersLinear[hit.num];
      row = hit.row;
      sector = hit.slice;
    } else if constexpr (std::is_same<T, TrackTPC>::value) {
      cl = &trkX.getCluster(mPtrackHitReferences, i, *mPclusterNative, sector, row);
    }
    float x, y, z;
    mPfastTransform->Transform(sector, row, cl->getPad(), cl->getTime(), x, y, z, zOffset);
    if constexpr (std::is_same<S, GPUTPCGMTrackParam>::value) {
      if (prop.PropagateToXAlpha(x, mPparam->Alpha(sector), !outward)) {
        return 1;
      }
      if (prop.Update(y, z, row, *mPparam, 0, 0, nullptr, false)) {
        return 1;
      }

    } else if constexpr (std::is_same<T, TrackParCov>::value) {
        
    }
  }

  convertTrack(trkX, trk, prop);
  return 0;
}

template GPUd() int GPUTrackingRefit::RefitTrack<GPUTPCGMMergedTrack, TrackParCov>(GPUTPCGMMergedTrack& trk, bool outward);
template GPUd() int GPUTrackingRefit::RefitTrack<TrackTPC, TrackParCov>(TrackTPC& trk, bool outward);
template GPUd() int GPUTrackingRefit::RefitTrack<GPUTPCGMMergedTrack, GPUTPCGMTrackParam>(GPUTPCGMMergedTrack& trk, bool outward);
template GPUd() int GPUTrackingRefit::RefitTrack<TrackTPC, GPUTPCGMTrackParam>(TrackTPC& trk, bool outward);

void GPUTrackingRefit::SetPtrsFromGPUConstantMem(const GPUConstantMem* v)
{
  //mPclusterState
  //mPpropagator
  mPclusterNative = v->ioPtrs.clustersNative;
  mPtrackHits = v->ioPtrs.mergedTrackHits;
  //mPtrackHitReferences
  mPfastTransform = v->calibObjects.fastTransform;
  mPparam = &v->param;
}
