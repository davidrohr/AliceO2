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

/// \file  TPCFastTransformGeo.h
/// \brief Definition of TPCFastTransformGeo class
///
/// \author  Sergey Gorbunov <sergey.gorbunov@cern.ch>

#ifndef ALICEO2_GPUCOMMON_TPCFASTTRANSFORMATION_TPCFASTTRANSFORMGEO_H
#define ALICEO2_GPUCOMMON_TPCFASTTRANSFORMATION_TPCFASTTRANSFORMGEO_H

#include "GPUCommonDef.h"
#ifndef GPUCA_GPUCODE_DEVICE
#include <memory>
#include "GPUCommonRtypes.h"
#endif

namespace o2
{
namespace gpu
{

///
/// The TPCFastTransformGeo class contains TPC geometry needed for the TPCFastTransform
///
class TPCFastTransformGeo
{
 public:
  /// The struct contains necessary info for TPC sector
  struct SectorInfo {
    float sinAlpha;
    float cosAlpha;
    ClassDefNV(SectorInfo, 1);
  };

  /// The struct contains necessary info about TPC padrow
  struct RowInfo {
    float x;          ///< nominal X coordinate of the row [cm]
    int32_t maxPad;   ///< maximal pad number = n pads - 1
    float padWidth;   ///< width of pads [cm]
    float u0;         ///< min. u coordinate
    float scaleUtoSU; ///< scale for su (scaled u ) coordinate
    float scaleSUtoU; ///< scale for u coordinate

    /// get width in U
    GPUd() float getUwidth() const { return -2.f * u0; }
    ClassDefNV(RowInfo, 1);
  };

  /// _____________  Constructors / destructors __________________________

  /// Default constructor: creates an empty uninitialized object
  TPCFastTransformGeo();

  /// Copy constructor: disabled to avoid ambiguity. Use cloneFromObject() instead
  TPCFastTransformGeo(const TPCFastTransformGeo&) = default;

  /// Assignment operator: disabled to avoid ambiguity. Use cloneFromObject() instead
  TPCFastTransformGeo& operator=(const TPCFastTransformGeo&) = default;

  /// Destructor
  ~TPCFastTransformGeo() = default;

  /// _____________  FlatObject functionality, see FlatObject class for description  ____________

  /// Gives minimal alignment in bytes required for an object of the class
  static constexpr size_t getClassAlignmentBytes() { return 8; }

  /// _______________  Construction interface  ________________________

  /// Starts the initialization procedure, reserves temporary memory
  void startConstruction(int32_t numberOfRows);

  /// Initializes a TPC row
  void setTPCrow(int32_t iRow, float x, int32_t nPads, float padWidth);

  /// Sets TPC geometry
  ///
  /// It must be called once during initialization
  void setTPCzLength(float tpcZlengthSideA, float tpcZlengthSideC);

  /// Sets all drift calibration parameters and the time stamp
  ///
  /// It must be called once during construction,
  /// but also may be called afterwards to reset these parameters.
  void setTPCalignmentZ(float tpcAlignmentZ);

  /// Finishes initialization: puts everything to the flat buffer, releases temporary memory
  void finishConstruction();

  /// Is the object constructed
  bool isConstructed() const { return (mConstructionMask == (uint32_t)ConstructionState::Constructed); }

  /// _______________  Getters _________________________________

  /// Gives number of TPC sectors
  GPUd() static constexpr int32_t getNumberOfSectors() { return NumberOfSectors; }

  /// Gives number of TPC sectors in A side
  GPUd() static constexpr int32_t getNumberOfSectorsA() { return NumberOfSectorsA; }

  /// Gives number of TPC rows
  GPUd() int32_t getNumberOfRows() const { return mNumberOfRows; }

  /// Gives sector info
  GPUd() const SectorInfo& getSectorInfo(int32_t sector) const;

  /// Gives TPC row info
  GPUd() const RowInfo& getRowInfo(int32_t row) const;

  /// Gives Z length of the TPC, side A
  GPUd() float getTPCzLengthA() const { return mTPCzLengthA; }

  /// Gives Z length of the TPC, side C
  GPUd() float getTPCzLengthC() const { return mTPCzLengthC; }

  /// Gives Z length of the TPC, depending on the sector
  GPUd() float getTPCzLength(int32_t sector) const
  {
    return (sector < NumberOfSectorsA) ? mTPCzLengthA
                                     : mTPCzLengthC;
  }

  /// Gives TPC alignment in Z
  GPUd() float getTPCalignmentZ() const { return mTPCalignmentZ; }

  /// _______________  Conversion of coordinate systems __________

  /// convert Local -> Global c.s.
  GPUd() void convLocalToGlobal(int32_t sector, float lx, float ly, float lz, float& gx, float& gy, float& gz) const;

  /// convert Global->Local c.s.
  GPUd() void convGlobalToLocal(int32_t sector, float gx, float gy, float gz, float& lx, float& ly, float& lz) const;

  /// convert UV -> Local c.s.
  GPUd() void convUVtoLocal(int32_t sector, float u, float v, float& y, float& z) const;
  GPUd() void convVtoLocal(int32_t sector, float v, float& z) const;

  /// convert Local-> UV c.s.
  GPUd() void convLocalToUV(int32_t sector, float y, float z, float& u, float& v) const;

  /// convert UV -> Scaled UV
  GPUd() void convUVtoScaledUV(int32_t sector, int32_t row, float u, float v, float& su, float& sv) const;

  /// convert Scaled UV -> UV
  GPUd() void convScaledUVtoUV(int32_t sector, int32_t row, float su, float sv, float& u, float& v) const;

  /// convert Scaled UV -> Local c.s.
  GPUd() void convScaledUVtoLocal(int32_t sector, int32_t row, float su, float sv, float& ly, float& lz) const;

  /// convert Pad coordinate -> U
  GPUd() float convPadToU(int32_t row, float pad) const;

  /// convert U -> Pad coordinate
  GPUd() float convUtoPad(int32_t row, float u) const;

  /// Print method
  void print() const;

  /// Method for testing consistency
  int32_t test(int32_t sector, int32_t row, float ly, float lz) const;

  /// Method for testing consistency
  int32_t test() const;

 private:
  /// _______________  Data members  _______________________________________________

  static constexpr int32_t NumberOfSectors = 36;                  ///< Number of TPC sectors ( sector = inner + outer sector )
  static constexpr int32_t NumberOfSectorsA = NumberOfSectors / 2; ///< Number of TPC sectors side A
  static constexpr int32_t MaxNumberOfRows = 160;                ///< Max Number of TPC rows in a sector

  /// _______________  Construction control  _______________________________________________

  /// Enumeration of construction states
  enum ConstructionState : uint32_t {
    NotConstructed = 0x0, ///< the object is not constructed
    Constructed = 0x1,    ///< the object is constructed, temporary memory is released
    InProgress = 0x2,     ///< construction started: temporary  memory is reserved
    GeometryIsSet = 0x4,  ///< the TPC geometry is set
    AlignmentIsSet = 0x8  ///< the TPC alignment is set
  };

  uint32_t mConstructionMask = ConstructionState::NotConstructed; ///< mask for constructed object members, first two bytes are used by this class

  /// _______________  Geometry  _______________________________________________

  int32_t mNumberOfRows = 0;    ///< Number of TPC rows. It is different for the Run2 and the Run3 setups
  float mTPCzLengthA = 0.f;     ///< Z length of the TPC, side A
  float mTPCzLengthC = 0.f;     ///< Z length of the TPC, side C
  float mTPCalignmentZ = 0.f;   ///< Global Z shift of the TPC detector. It is applied at the end of the transformation.
  float mScaleVtoSVsideA = 0.f; ///< scale for v->sv for TPC side A
  float mScaleVtoSVsideC = 0.f; ///< scale for v->sv for TPC side C
  float mScaleSVtoVsideA = 0.f; ///< scale for sv->v for TPC side A
  float mScaleSVtoVsideC = 0.f; ///< scale for sv->v for TPC side C

  SectorInfo mSectorInfos[NumberOfSectors + 1]; ///< array of sector information [fixed size]
  RowInfo mRowInfos[MaxNumberOfRows + 1];    ///< array of row information [fixed size]

  ClassDefNV(TPCFastTransformGeo, 1);
};

// =======================================================================
//              Inline implementations of some methods
// =======================================================================

GPUdi() const TPCFastTransformGeo::SectorInfo& TPCFastTransformGeo::getSectorInfo(int32_t sector) const
{
  /// Gives sector info
  if (sector < 0 || sector >= NumberOfSectors) { // return zero object
    sector = NumberOfSectors;
  }
  return mSectorInfos[sector];
}

GPUdi() const TPCFastTransformGeo::RowInfo& TPCFastTransformGeo::getRowInfo(int32_t row) const
{
  /// Gives TPC row info
  if (row < 0 || row >= mNumberOfRows) { // return zero object
    row = MaxNumberOfRows;
  }
  return mRowInfos[row];
}

GPUdi() void TPCFastTransformGeo::convLocalToGlobal(int32_t sector, float lx, float ly, float lz, float& gx, float& gy, float& gz) const
{
  /// convert Local -> Global c.s.
  const SectorInfo& sectorInfo = getSectorInfo(sector);
  gx = lx * sectorInfo.cosAlpha - ly * sectorInfo.sinAlpha;
  gy = lx * sectorInfo.sinAlpha + ly * sectorInfo.cosAlpha;
  gz = lz;
}

GPUdi() void TPCFastTransformGeo::convGlobalToLocal(int32_t sector, float gx, float gy, float gz, float& lx, float& ly, float& lz) const
{
  /// convert Global -> Local c.s.
  const SectorInfo& sectorInfo = getSectorInfo(sector);
  lx = gx * sectorInfo.cosAlpha + gy * sectorInfo.sinAlpha;
  ly = -gx * sectorInfo.sinAlpha + gy * sectorInfo.cosAlpha;
  lz = gz;
}

GPUdi() void TPCFastTransformGeo::convVtoLocal(int32_t sector, float v, float& lz) const
{
  /// convert UV -> Local c.s.
  if (sector < NumberOfSectorsA) { // TPC side A
    lz = mTPCzLengthA - v;
  } else {                 // TPC side C
    lz = v - mTPCzLengthC; // drift direction is mirrored on C-side
  }
  lz += mTPCalignmentZ; // global TPC alignment
}

GPUdi() void TPCFastTransformGeo::convUVtoLocal(int32_t sector, float u, float v, float& ly, float& lz) const
{
  /// convert UV -> Local c.s.
  if (sector < NumberOfSectorsA) { // TPC side A
    ly = u;
    lz = mTPCzLengthA - v;
  } else {                 // TPC side C
    ly = -u;               // pads are mirrorred on C-side
    lz = v - mTPCzLengthC; // drift direction is mirrored on C-side
  }
  lz += mTPCalignmentZ; // global TPC alignment
}

GPUdi() void TPCFastTransformGeo::convLocalToUV(int32_t sector, float ly, float lz, float& u, float& v) const
{
  /// convert Local-> UV c.s.
  lz = lz - mTPCalignmentZ;      // global TPC alignment
  if (sector < NumberOfSectorsA) { // TPC side A
    u = ly;
    v = mTPCzLengthA - lz;
  } else {                 // TPC side C
    u = -ly;               // pads are mirrorred on C-side
    v = lz + mTPCzLengthC; // drift direction is mirrored on C-side
  }
}

GPUdi() void TPCFastTransformGeo::convUVtoScaledUV(int32_t sector, int32_t row, float u, float v, float& su, float& sv) const
{
  /// convert UV -> Scaled UV
  const RowInfo& rowInfo = getRowInfo(row);
  su = (u - rowInfo.u0) * rowInfo.scaleUtoSU;
  if (sector < NumberOfSectorsA) {
    sv = v * mScaleVtoSVsideA;
  } else {
    sv = v * mScaleVtoSVsideC;
  }
}

GPUdi() void TPCFastTransformGeo::convScaledUVtoUV(int32_t sector, int32_t row, float su, float sv, float& u, float& v) const
{
  /// convert Scaled UV -> UV
  const RowInfo& rowInfo = getRowInfo(row);
  u = rowInfo.u0 + su * rowInfo.scaleSUtoU;
  if (sector < NumberOfSectorsA) {
    v = sv * mScaleSVtoVsideA;
  } else {
    v = sv * mScaleSVtoVsideC;
  }
}

GPUdi() void TPCFastTransformGeo::convScaledUVtoLocal(int32_t sector, int32_t row, float su, float sv, float& ly, float& lz) const
{
  /// convert Scaled UV -> Local c.s.
  float u, v;
  convScaledUVtoUV(sector, row, su, sv, u, v);
  convUVtoLocal(sector, u, v, ly, lz);
}

GPUdi() float TPCFastTransformGeo::convPadToU(int32_t row, float pad) const
{
  /// convert Pad coordinate -> U
  const RowInfo& rowInfo = getRowInfo(row);
  return (pad - 0.5f * rowInfo.maxPad) * rowInfo.padWidth;
}

GPUdi() float TPCFastTransformGeo::convUtoPad(int32_t row, float u) const
{
  /// convert U -> Pad coordinate
  const RowInfo& rowInfo = getRowInfo(row);
  return u / rowInfo.padWidth + 0.5f * rowInfo.maxPad;
}

} // namespace gpu
} // namespace o2

#endif
