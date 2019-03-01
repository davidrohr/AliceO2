// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUTPCBaseTrackParam.h
/// \author David Rohr, Sergey Gorbunov

#ifndef GPUTPCBASETRACKPARAM_H
#define GPUTPCBASETRACKPARAM_H

#include "GPUTPCDef.h"
MEM_CLASS_PRE()
class GPUTPCTrackParam;

/**
 * @class GPUTPCBaseTrackParam
 *
 * GPUTPCBaseTrackParam class contains track parameters
 * used in output of the GPUTPCTracker slice tracker.
 * This class is used for transfer between tracker and merger and does not contain the covariance matrice
 */
MEM_CLASS_PRE()
class GPUTPCBaseTrackParam
{
  public:
	GPUd() float X() const { return fX; }
	GPUd() float Y() const { return fP[0]; }
	GPUd() float Z() const { return fP[1]; }
	GPUd() float SinPhi() const { return fP[2]; }
	GPUd() float DzDs() const { return fP[3]; }
	GPUd() float QPt() const { return fP[4]; }
	GPUd() float ZOffset() const { return fZOffset; }

	GPUhd() float GetX() const { return fX; }
	GPUhd() float GetY() const { return fP[0]; }
	GPUhd() float GetZ() const { return fP[1]; }
	GPUhd() float GetSinPhi() const { return fP[2]; }
	GPUhd() float GetDzDs() const { return fP[3]; }
	GPUhd() float GetQPt() const { return fP[4]; }
	GPUhd() float GetZOffset() const { return fZOffset; }

	GPUd() float GetKappa(float Bz) const { return -fP[4] * Bz; }

	GPUhd() MakeType(const float *) Par() const { return fP; }
	GPUd() const MakeType(float *) GetPar() const { return fP; }
	GPUd() float GetPar(int i) const { return (fP[i]); }

	GPUhd() void SetPar(int i, float v) { fP[i] = v; }

	GPUd() void SetX(float v) { fX = v; }
	GPUd() void SetY(float v) { fP[0] = v; }
	GPUd() void SetZ(float v) { fP[1] = v; }
	GPUd() void SetSinPhi(float v) { fP[2] = v; }
	GPUd() void SetDzDs(float v) { fP[3] = v; }
	GPUd() void SetQPt(float v) { fP[4] = v; }
	GPUd() void SetZOffset(float v) { fZOffset = v; }

  private:
	//WARNING, Track Param Data is copied in the GPU Tracklet Constructor element by element instead of using copy constructor!!!
	//This is neccessary for performance reasons!!!
	//Changes to Elements of this class therefore must also be applied to TrackletConstructor!!!
	float fX; // x position
	float fZOffset;
	float fP[5]; // 'active' track parameters: Y, Z, SinPhi, DzDs, q/Pt
};

#endif
