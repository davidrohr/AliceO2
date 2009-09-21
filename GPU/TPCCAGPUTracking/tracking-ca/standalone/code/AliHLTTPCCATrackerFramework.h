//-*- Mode: C++ -*-
// @(#) $Id: AliHLTTPCCATracker.h 33907 2009-07-23 13:52:49Z sgorbuno $
// ************************************************************************
// This file is property of and copyright by the ALICE HLT Project        *
// ALICE Experiment at CERN, All rights reserved.                         *
// See cxx source for full Copyright notice                               *
//                                                                        *
//*************************************************************************

#ifndef ALIHLTTPCCATRACKERFRAMEWORK_H
#define ALIHLTTPCCATRACKERFRAMEWORK_H

#include "AliHLTTPCCATracker.h"
#include "AliHLTTPCCAGPUTracker.h"
#include "AliHLTTPCCASliceOutput.h"
#include "AliHLTTPCCAClusterData.h"
#include "AliHLTTPCCAParam.h"

class AliHLTTPCCATrackerFramework
{
public:
	AliHLTTPCCATrackerFramework() :
	  fGPUTrackerAvailable(false), fUseGPUTracker(false), fGPUDebugLevel(0), fGPUSliceCount(0), fGPUTracker(), fCPUSliceCount(fgkNSlices)
	  {}
    ~AliHLTTPCCATrackerFramework()
	  {}

	int InitGPU(int sliceCount = 1, int forceDeviceID = -1);
	int ExitGPU();
	void SetGPUDebugLevel(int Level, std::ostream *OutFile = NULL, std::ostream *GPUOutFile = NULL);
	int SetGPUTrackerOption(char* OptionName, int OptionValue) {return(fGPUTracker.SetGPUTrackerOption(OptionName, OptionValue));}
	int SetGPUTracker(bool enable);

	int InitializeSliceParam(int iSlice, AliHLTTPCCAParam &param);

	int ProcessSlices(int firstSlice, int sliceCount, AliHLTTPCCAClusterData* pClusterData, AliHLTTPCCASliceOutput* pOutput);
	unsigned long long int* PerfTimer(int GPU, int iSlice, int iTimer);

	int MaxSliceCount() { return(fUseGPUTracker ? fGPUSliceCount : fCPUSliceCount); }

	const AliHLTTPCCAParam& Param(int iSlice) const { return(fCPUTrackers[iSlice].Param()); }
	const AliHLTTPCCARow& Row(int iSlice, int iRow) const { return(fCPUTrackers[iSlice].Row(iRow)); }  //TODO: Should be changed to return only row parameters

private:
  static const int fgkNSlices = 36;       //* N slices

  bool fGPUTrackerAvailable; // Is the GPU Tracker Available?
  bool fUseGPUTracker; // use the GPU tracker 
  int fGPUDebugLevel;  // debug level for the GPU code
  int fGPUSliceCount;	//How many slices to process parallel
  AliHLTTPCCAGPUTracker fGPUTracker;

  AliHLTTPCCATracker fCPUTrackers[fgkNSlices];
  int fCPUSliceCount;

  AliHLTTPCCATrackerFramework( const AliHLTTPCCATrackerFramework& );
  AliHLTTPCCATrackerFramework &operator=( const AliHLTTPCCATrackerFramework& );

};

#endif