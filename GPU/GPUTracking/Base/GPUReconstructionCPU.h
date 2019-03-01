// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUReconstructionCPU.h
/// \author David Rohr

#ifndef GPURECONSTRUCTIONIMPL_H
#define GPURECONSTRUCTIONIMPL_H

#include "GPUReconstruction.h"
#include "GPUReconstructionHelpers.h"
#include "GPUConstantMem.h"
#include <stdexcept>
#include "utils/timer.h"
#include <vector>

#include "GPUGeneralKernels.h"
#include "GPUTPCNeighboursFinder.h"
#include "GPUTPCNeighboursCleaner.h"
#include "GPUTPCStartHitsFinder.h"
#include "GPUTPCStartHitsSorter.h"
#include "GPUTPCTrackletConstructor.h"
#include "GPUTPCTrackletSelector.h"
#include "GPUTPCGMMergerGPU.h"
#include "GPUTRDTrackerGPU.h"
#include "GPUITSFitterKernels.h"

namespace GPUReconstruction_krnlHelpers {
template <class T, int I = 0> class classArgument {};

typedef void deviceEvent; //We use only pointers anyway, and since cl_event and cudaEvent_t are actually pointers, we can cast them to deviceEvent* this way.

enum class krnlDeviceType : int {CPU = 0, Device = 1, Auto = -1};
struct krnlExec
{
	krnlExec(unsigned int b, unsigned int t, int s, krnlDeviceType d = krnlDeviceType::Auto) : nBlocks(b), nThreads(t), stream(s), device(d) {}
	unsigned int nBlocks;
	unsigned int nThreads;
	int stream;
	krnlDeviceType device;
};
struct krnlRunRange
{
	krnlRunRange() : start(0), num(0) {}
	krnlRunRange(unsigned int a) : start(a), num(0) {}
	krnlRunRange(unsigned int s, int n) : start(s), num(n) {}
	
	unsigned int start;
	int num;
};
static const krnlRunRange krnlRunRangeNone(0, -1);
struct krnlEvent
{
	krnlEvent(deviceEvent* e = nullptr, deviceEvent* el = nullptr, int n = 1) : ev(e), evList(el), nEvents(n) {}
	deviceEvent* ev;
	deviceEvent* evList;
	int nEvents;
};
static const krnlEvent krnlEventNone{};
} //End Namespace

using namespace GPUReconstruction_krnlHelpers;

class GPUReconstructionCPUBackend : public GPUReconstruction
{
public:
	virtual ~GPUReconstructionCPUBackend() = default;
	
protected:
	GPUReconstructionCPUBackend(const GPUSettingsProcessing& cfg) : GPUReconstruction(cfg) {}
	template <class T, int I = 0, typename... Args> int runKernelBackend(const krnlExec& x, const krnlRunRange& y, const krnlEvent& z, const Args&... args);
};

#include "GPUReconstructionKernels.h"
#ifndef GPUCA_GPURECONSTRUCTIONCPU_IMPLEMENTATION
	#define GPUCA_GPURECONSTRUCTIONCPU_DECLONLY
	#undef GPURECONSTRUCTIONKERNELS_H
	#include "GPUReconstructionKernels.h"
#endif

class GPUReconstructionCPU : public GPUReconstructionKernels<GPUReconstructionCPUBackend>
{
	friend class GPUReconstruction;
	friend class GPUChain;
	
public:
	virtual ~GPUReconstructionCPU() = default;

#ifdef __APPLE__ //MacOS compiler BUG: clang seems broken and does not accept default parameters before parameter pack
	template <class S, int I = 0> inline int runKernel(const krnlExec& x, HighResTimer* t = nullptr, const krnlRunRange& y = krnlRunRangeNone)
	{
		return runKernel<S, I>(x, t, y, krnlEvent());
	}
	template <class S, int I = 0, typename... Args> inline int runKernel(const krnlExec& x, HighResTimer* t, const krnlRunRange& y, const krnlEvent& z, const Args&... args)
#else
	template <class S, int I = 0, typename... Args> inline int runKernel(const krnlExec& x, HighResTimer* t = nullptr, const krnlRunRange& y = krnlRunRangeNone, const krnlEvent& z = krnlEvent(), const Args&... args)
#endif
	{
		if (mDeviceProcessingSettings.debugLevel >= 3) printf("Running %s Stream %d (Range %d/%d)\n", typeid(S).name(), x.stream, y.start, y.num);
		if (t && mDeviceProcessingSettings.debugLevel) t->Start();
		if (IsGPU() && (mRecoStepsGPU & S::GetRecoStep()) != S::GetRecoStep())
		{
			if (mDeviceProcessingSettings.debugLevel >= 4) printf("Running unsupported kernel on CPU: %s\n", typeid(S).name());
			if (GPUReconstructionCPU::runKernelImpl(classArgument<S, I>(), x, y, z, args...)) return 1;
		}
		else
		{
			if (runKernelImpl(classArgument<S, I>(), x, y, z, args...)) return 1;
		}
		if (mDeviceProcessingSettings.debugLevel)
		{
			if (GPUDebug(typeid(S).name(), x.stream)) throw std::runtime_error("kernel failure");
			if (t) t->Stop();
		}
		return 0;
	}
	
	virtual int GPUDebug(const char* state = "UNKNOWN", int stream = -1);
	void TransferMemoryResourceToGPU(GPUMemoryResource* res, int stream = -1, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int nEvents = 1) {TransferMemoryInternal(res, stream, ev, evList, nEvents, true, res->Ptr(), res->PtrDevice());}
	void TransferMemoryResourceToHost(GPUMemoryResource* res, int stream = -1, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int nEvents = 1) {TransferMemoryInternal(res, stream, ev, evList, nEvents, false, res->PtrDevice(), res->Ptr());}
	void TransferMemoryResourcesToGPU(GPUProcessor* proc, int stream = -1, bool all = false) {TransferMemoryResourcesHelper(proc, stream, all, true);}
	void TransferMemoryResourcesToHost(GPUProcessor* proc, int stream = -1, bool all = false) {TransferMemoryResourcesHelper(proc, stream, all, false);}
	void TransferMemoryResourceLinkToGPU(short res, int stream = -1, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int nEvents = 1) {TransferMemoryResourceToGPU(&mMemoryResources[res], stream, ev, evList, nEvents);}
	void TransferMemoryResourceLinkToHost(short res, int stream = -1, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int nEvents = 1) {TransferMemoryResourceToHost(&mMemoryResources[res], stream, ev, evList, nEvents);}
	virtual void WriteToConstantMemory(size_t offset, const void* src, size_t size, int stream = -1, deviceEvent* ev = nullptr);
	int GPUStuck() {return mGPUStuck;}
	int NStreams() {return mNStreams;}
	void SetThreadCounts(RecoStep step);
	void ResetDeviceProcessorTypes();
	template <class T> void AddGPUEvents(T& events);

	virtual int RunStandalone() override;
	
protected:
	struct GPUProcessorWorkers : public GPUProcessor
	{
		GPUConstantMem* mWorkersProc = nullptr;
		void* SetPointersDeviceProcessor(void* mem);
		short mMemoryResWorkers = -1;
	};

	GPUReconstructionCPU(const GPUSettingsProcessing& cfg) : GPUReconstructionKernels(cfg) {}

	virtual void SynchronizeStream(int stream) {}
	virtual void SynchronizeEvents(deviceEvent* evList, int nEvents = 1) {}
	virtual bool IsEventDone(deviceEvent* evList, int nEvents = 1) {return true;}
	virtual void RecordMarker(deviceEvent* ev, int stream) {}
	virtual void ActivateThreadContext() {}
	virtual void ReleaseThreadContext() {}
	virtual void SynchronizeGPU() {}
	virtual void ReleaseEvent(deviceEvent* ev) {}
	virtual int StartHelperThreads() {return 0;}
	virtual int StopHelperThreads() {return 0;}
	virtual void RunHelperThreads(int (GPUReconstructionHelpers::helperDelegateBase::* function)(int, int, GPUReconstructionHelpers::helperParam*), GPUReconstructionHelpers::helperDelegateBase* functionCls, int count) {}
	virtual void WaitForHelperThreads() {}
	virtual int HelperError(int iThread) const {return 0;}
	virtual int HelperDone(int iThread) const {return 0;}
	virtual void ResetHelperThreads(int helpers) {}
	virtual void TransferMemoryInternal(GPUMemoryResource* res, int stream, deviceEvent* ev, deviceEvent* evList, int nEvents, bool toGPU, void* src, void* dst);
	
	virtual void SetThreadCounts();
	
	virtual int InitDevice() override;
	virtual int ExitDevice() override;
	int GetThread();
	
	virtual int PrepareTextures() {return 0;}
	virtual int DoStuckProtection(int stream, void* event) {return 0;}
	
	//Pointers to tracker classes
	GPUProcessorWorkers mProcShadow; //Host copy of tracker objects that will be used on the GPU
	GPUConstantMem* &mWorkersShadow = mProcShadow.mWorkersProc;
	
	unsigned int fBlockCount = 0;                 //Default GPU block count
	unsigned int fThreadCount = 0;                //Default GPU thread count
	unsigned int fConstructorBlockCount = 0;      //GPU blocks used in Tracklet Constructor
	unsigned int fSelectorBlockCount = 0;         //GPU blocks used in Tracklet Selector
	unsigned int fConstructorThreadCount = 0;
	unsigned int fSelectorThreadCount = 0;
	unsigned int fFinderThreadCount = 0;
	unsigned int fTRDThreadCount = 0;

	int mThreadId = -1; //Thread ID that is valid for the local CUDA context
	int mGPUStuck = 0;		//Marks that the GPU is stuck, skip future events
	int mNStreams = 1;
	
	GPUConstantMem* mDeviceConstantMem = nullptr;
	std::vector<std::pair<deviceEvent*, size_t>> mEvents;
	
private:
	void TransferMemoryResourcesHelper(GPUProcessor* proc, int stream, bool all, bool toGPU);
};

template <class T> inline void GPUReconstructionCPU::AddGPUEvents(T& events)
{
	mEvents.emplace_back((void*) &events, sizeof(T) / sizeof(deviceEvent*));
}

#endif
