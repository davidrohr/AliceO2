// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUDataTypes.h
/// \author David Rohr

#ifndef GPUDATATYPES_H
#define GPUDATATYPES_H

#if (!defined(__OPENCL__) || defined(__OPENCLCPP__)) && !(defined(__CINT__) || defined(__ROOTCINT__))
#define ENUM_CLASS class
#define ENUM_UINT : unsigned int
#define GPUCA_RECO_STEP GPUDataTypes::RecoStep
#else
#define ENUM_CLASS
#define ENUM_UINT
#define GPUCA_RECO_STEP GPUDataTypes
#endif

class GPUDataTypes
{
 public:
  enum ENUM_CLASS GeometryType ENUM_UINT{ RESERVED_GEOMETRY = 0, ALIROOT = 1, O2 = 2 };
  enum DeviceType ENUM_UINT { INVALID_DEVICE = 0,
                              CPU = 1,
                              CUDA = 2,
                              HIP = 3,
                              OCL = 4 };
  enum ENUM_CLASS RecoStep { TPCSliceTracking = 1,
                             TPCMerging = 2,
                             TRDTracking = 4,
                             ITSTracking = 8,
                             AllRecoSteps = 0x7FFFFFFF,
                             NoRecoStep = 0 };
};

#undef ENUM_CLASS
#undef ENUM_UINT

#endif
