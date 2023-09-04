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

/// \file   CTFHelper.h
/// \author ruben.shahoyan@cern.ch
/// \brief  Helper for PHOS CTF creation

#ifndef O2_PHOS_CTF_HELPER_H
#define O2_PHOS_CTF_HELPER_H

#include "DataFormatsPHOS/CTF.h"
#include <gsl/span>

namespace o2
{
namespace phos
{

class CTFHelper
{

 public:
  CTFHelper(const gsl::span<const TriggerRecord>& trgData, const gsl::span<const Cell>& cellData)
    : mTrigData(trgData), mCellData(cellData) {}

  CTFHeader createHeader()
  {
    CTFHeader h{o2::detectors::DetID::PHS, 0, 1, 0, // dummy timestamp, version 1.0
                uint32_t(mTrigData.size()), uint32_t(mCellData.size()), 0, 0};
    if (mTrigData.size()) {
      h.firstOrbit = mTrigData[0].getBCData().orbit;
      h.firstBC = mTrigData[0].getBCData().bc;
    }
    return h;
  }

  size_t getSize() const { return mTrigData.size() * sizeof(TriggerRecord) + mCellData.size() * sizeof(Cell); }

  //>>> =========================== ITERATORS ========================================

  template <typename I, typename D, typename T>
  class _Iter
  {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = const T*;
    using reference = const T&;
    using iterator_category = std::random_access_iterator_tag;

    _Iter(const gsl::span<const D>& data, bool end = false) : mData(data), mIndex(end ? data.size() : 0){};
    _Iter() = default;

    inline I& operator++() noexcept
    {
      ++mIndex;
      return static_cast<I&>(*this);
    }

    inline I operator++(int)
    {
      I res = *(static_cast<I*>(this));
      ++mIndex;
      return res;
    }

    inline I& operator--() noexcept
    {
      mIndex--;
      return static_cast<I&>(*this);
    }

    inline I operator--(int)
    {
      I res = *(static_cast<I*>(this));
      --mIndex;
      return res;
    }

    I& operator+=(difference_type i) noexcept
    {
      mIndex += i;
      return static_cast<I&>(*this);
    }

    I operator+(difference_type i) const
    {
      I res = *(const_cast<I*>(static_cast<const I*>(this)));
      return res += i;
    }

    I& operator-=(difference_type i) noexcept
    {
      mIndex -= i;
      return static_cast<I&>(*this);
    }

    I operator-(difference_type i) const
    {
      I res = *(const_cast<I*>(static_cast<const I*>(this)));
      return res -= i;
    }

    difference_type operator-(const I& other) const noexcept { return mIndex - other.mIndex; }

    inline friend I operator+(difference_type i, const I& iter) { return iter + i; };

    bool operator!=(const I& other) const noexcept { return mIndex != other.mIndex; }
    bool operator==(const I& other) const noexcept { return mIndex == other.mIndex; }
    bool operator>(const I& other) const noexcept { return mIndex > other.mIndex; }
    bool operator<(const I& other) const noexcept { return mIndex < other.mIndex; }
    bool operator>=(const I& other) const noexcept { return mIndex >= other.mIndex; }
    bool operator<=(const I& other) const noexcept { return mIndex <= other.mIndex; }

   protected:
    gsl::span<const D> mData{};
    difference_type mIndex = 0;
  };

  //_______________________________________________
  // BC difference wrt previous if in the same orbit, otherwise the abs.value.
  // For the very 1st entry return 0 (diff wrt 1st BC in the CTF header)
  class Iter_bcIncTrig : public _Iter<Iter_bcIncTrig, TriggerRecord, int16_t>
  {
   public:
    using _Iter<Iter_bcIncTrig, TriggerRecord, int16_t>::_Iter;
    value_type operator*() const
    {
      if (mIndex) {
        if (mData[mIndex].getBCData().orbit == mData[mIndex - 1].getBCData().orbit) {
          return value_type(mData[mIndex].getBCData().bc - mData[mIndex - 1].getBCData().bc);
        } else {
          return value_type(mData[mIndex].getBCData().bc);
        }
      }
      return 0;
    }
    value_type operator[](difference_type i) const
    {
      size_t id = mIndex + i;
      if (id) {
        if (mData[id].getBCData().orbit == mData[id - 1].getBCData().orbit) {
          return value_type(mData[id].getBCData().bc - mData[id - 1].getBCData().bc);
        } else {
          return value_type(mData[id].getBCData().bc);
        }
      }
      return 0;
    }
  };

  //_______________________________________________
  // Orbit difference wrt previous. For the very 1st entry return 0 (diff wrt 1st BC in the CTF header)
  class Iter_orbitIncTrig : public _Iter<Iter_orbitIncTrig, TriggerRecord, int32_t>
  {
   public:
    using _Iter<Iter_orbitIncTrig, TriggerRecord, int32_t>::_Iter;
    value_type operator*() const { return value_type(mIndex ? mData[mIndex].getBCData().orbit - mData[mIndex - 1].getBCData().orbit : 0); }
    value_type operator[](difference_type i) const
    {
      size_t id = mIndex + i;
      return value_type(id ? mData[id].getBCData().orbit - mData[id - 1].getBCData().orbit : 0);
    }
  };

  //_______________________________________________
  // Number of cells for trigger
  class Iter_entriesTrig : public _Iter<Iter_entriesTrig, TriggerRecord, uint16_t>
  {
   public:
    using _Iter<Iter_entriesTrig, TriggerRecord, uint16_t>::_Iter;
    value_type operator*() const { return mData[mIndex].getNumberOfObjects(); }
    value_type operator[](difference_type i) const { return mData[mIndex + i].getNumberOfObjects(); }
  };

  //_______________________________________________
  class Iter_packedID : public _Iter<Iter_packedID, Cell, uint16_t>
  {
   public:
    using _Iter<Iter_packedID, Cell, uint16_t>::_Iter;
    value_type operator*() const { return mData[mIndex].getPackedID(); }
    value_type operator[](difference_type i) const { return mData[mIndex + i].getPackedID(); }
  };

  //_______________________________________________
  class Iter_time : public _Iter<Iter_time, Cell, uint16_t>
  {
   public:
    using _Iter<Iter_time, Cell, uint16_t>::_Iter;
    value_type operator*() const { return mData[mIndex].getPackedTime(); }
    value_type operator[](difference_type i) const { return mData[mIndex + i].getPackedTime(); }
  };

  //_______________________________________________
  class Iter_energy : public _Iter<Iter_energy, Cell, uint16_t>
  {
   public:
    using _Iter<Iter_energy, Cell, uint16_t>::_Iter;
    value_type operator*() const { return mData[mIndex].getPackedEnergy(); }
    value_type operator[](difference_type i) const { return mData[mIndex + i].getPackedEnergy(); }
  };

  //_______________________________________________
  class Iter_status : public _Iter<Iter_status, Cell, uint8_t>
  {
   public:
    using _Iter<Iter_status, Cell, uint8_t>::_Iter;
    value_type operator*() const { return mData[mIndex].getPackedCellStatus(); }
    value_type operator[](difference_type i) const { return mData[mIndex + i].getPackedCellStatus(); }
  };

  //<<< =========================== ITERATORS ========================================

  Iter_bcIncTrig begin_bcIncTrig() const { return Iter_bcIncTrig(mTrigData, false); }
  Iter_bcIncTrig end_bcIncTrig() const { return Iter_bcIncTrig(mTrigData, true); }

  Iter_orbitIncTrig begin_orbitIncTrig() const { return Iter_orbitIncTrig(mTrigData, false); }
  Iter_orbitIncTrig end_orbitIncTrig() const { return Iter_orbitIncTrig(mTrigData, true); }

  Iter_entriesTrig begin_entriesTrig() const { return Iter_entriesTrig(mTrigData, false); }
  Iter_entriesTrig end_entriesTrig() const { return Iter_entriesTrig(mTrigData, true); }

  Iter_packedID begin_packedID() const { return Iter_packedID(mCellData, false); }
  Iter_packedID end_packedID() const { return Iter_packedID(mCellData, true); }

  Iter_time begin_time() const { return Iter_time(mCellData, false); }
  Iter_time end_time() const { return Iter_time(mCellData, true); }

  Iter_energy begin_energy() const { return Iter_energy(mCellData, false); }
  Iter_energy end_energy() const { return Iter_energy(mCellData, true); }

  Iter_status begin_status() const { return Iter_status(mCellData, false); }
  Iter_status end_status() const { return Iter_status(mCellData, true); }

 private:
  const gsl::span<const o2::phos::TriggerRecord> mTrigData;
  const gsl::span<const o2::phos::Cell> mCellData;
};

} // namespace phos
} // namespace o2

#endif
