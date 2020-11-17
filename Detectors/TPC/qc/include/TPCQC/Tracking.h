// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// @file   Tracking.h
/// @author David Rohr
///

#ifndef AliceO2_TPC_QC_TRACKING_H
#define AliceO2_TPC_QC_TRACKING_H

#include <vector>
#include <memory>

class TH1F;
class TH2F;
class TH1D;

//o2 includes
#include "DataFormatsTPC/Defs.h"

namespace o2
{
class MCCompLabel;
namespace gpu
{
class GPUO2InterfaceQA;
} // namespace gpu
namespace tpc
{
class TrackTPC;
struct ClusterNativeAccess;

namespace qc
{
class Tracking
{
 public:
  /// default constructor
  Tracking();
  ~Tracking();

  void processTracks(const std::vector<o2::tpc::TrackTPC>* tracks, const std::vector<o2::MCCompLabel>* tracksMC, const o2::tpc::ClusterNativeAccess* clNative, TObjArray* out = nullptr);

  enum outputModes {
    outputMergeable,
    outputPostprocessed,
    outputLayout
  };

  /// Initialize all histograms
  void initialize(outputModes outputMode);

  /// Reset all histograms
  void resetHistograms();

  /// get histograms
  void getHists(const std::vector<TH1F>*& h1, const std::vector<TH2F>*& h2, const std::vector<TH1D>*& h3) const;

 private:
  std::unique_ptr<o2::gpu::GPUO2InterfaceQA> mQA; //!
  outputModes mOutputMode;

  ClassDefNV(Tracking, 1)
};
} // namespace qc
} // namespace tpc
} // namespace o2

#endif
