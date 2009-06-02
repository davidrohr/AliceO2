//-*- Mode: C++ -*-
// ************************************************************************
// This file is property of and copyright by the ALICE HLT Project        *
// ALICE Experiment at CERN, All rights reserved.                         *
// See cxx source for full Copyright notice                               *
//                                                                        *
//*************************************************************************

#ifndef ALIHLTTPCCATRACKLETSELECTOR_H
#define ALIHLTTPCCATRACKLETSELECTOR_H


#include "AliHLTTPCCADef.h"
class AliHLTTPCCATracker;

/**
 * @class AliHLTTPCCATrackletSelector
 *
 */
class AliHLTTPCCATrackletSelector
{
  public:
    class AliHLTTPCCASharedMemory
    {
        friend class AliHLTTPCCATrackletSelector;
#ifndef CUDA_DEVICE_EMULATION
      protected:
#else
	  public:
#endif
        int fItr0; // index of the first track in the block
        int fNThreadsTotal; // total n threads
        int fNTracklets; // n of tracklets
    };

    GPUd() static int NThreadSyncPoints() { return 1; }

    GPUd() static void Thread( int nBlocks, int nThreads, int iBlock, int iThread, int iSync,
                               AliHLTTPCCASharedMemory &smem, AliHLTTPCCATracker &tracker );

};


#endif
