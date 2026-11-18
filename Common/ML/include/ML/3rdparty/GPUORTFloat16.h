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

/// \author David Rohr
/// Temporary file to support O2Physics code that uses it, should migrate away from this.

#ifndef GPUORTFLOAT16_H
#define GPUORTFLOAT16_H

#if __cplusplus >= 202302L && __has_include(<stdfloat>)
#include <stdfloat>
#endif

namespace o2::OrtDataType
{
#elif __cplusplus >= 202302L && defined(__STDCPP_FLOAT16_T__)
  using Float16_t = std::float16_t;
#else
  using Float16_t = _Float16;
#endif
}

#endif
