#ifndef ALIGPURECONSTRUCTIONIMPL_H
#define ALIGPURECONSTRUCTIONIMPL_H

#include "AliGPUReconstruction.h"
#include "AliGPUCADataTypes.h"
#include <stdexcept>
#include "cmodules/timer.h"

#include "AliGPUGeneralKernels.h"
#include "AliGPUTPCNeighboursFinder.h"
#include "AliGPUTPCNeighboursCleaner.h"
#include "AliGPUTPCStartHitsFinder.h"
#include "AliGPUTPCStartHitsSorter.h"
#include "AliGPUTPCTrackletConstructor.h"
#include "AliGPUTPCTrackletSelector.h"
#include "AliGPUTPCGMMergerGPU.h"
#include "AliGPUTRDTrackerGPU.h"

namespace AliGPUReconstruction_krnlHelpers {
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
} //End Namespace

using namespace AliGPUReconstruction_krnlHelpers;

template <class T> class AliGPUReconstructionImpl : public T
{
public:
	virtual ~AliGPUReconstructionImpl() = default;
	AliGPUReconstructionImpl(const AliGPUCASettingsProcessing& cfg) : T(cfg) {}
	AliGPUReconstructionImpl() = default;

protected:
	virtual int runKernelImpl(classArgument<AliGPUTPCNeighboursFinder>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTPCNeighboursFinder>(x, y, z);}
	virtual int runKernelImpl(classArgument<AliGPUTPCNeighboursCleaner>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTPCNeighboursCleaner>(x, y, z);}
	virtual int runKernelImpl(classArgument<AliGPUTPCStartHitsFinder>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTPCStartHitsFinder>(x, y, z);}
	virtual int runKernelImpl(classArgument<AliGPUTPCStartHitsSorter>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTPCStartHitsSorter>(x, y, z);}
	virtual int runKernelImpl(classArgument<AliGPUTPCTrackletConstructor>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTPCTrackletConstructor>(x, y, z);}
	virtual int runKernelImpl(classArgument<AliGPUTPCTrackletConstructor, 1>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTPCTrackletConstructor, 1>(x, y, z);}
	virtual int runKernelImpl(classArgument<AliGPUTPCTrackletSelector>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTPCTrackletSelector>(x, y, z);}
	virtual int runKernelImpl(classArgument<AliGPUMemClean16>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z, void* ptr, unsigned long size) {return T::template runKernelBackend<AliGPUMemClean16>(x, y, z, ptr, size);}
	virtual int runKernelImpl(classArgument<AliGPUTPCGMMergerTrackFit>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTPCGMMergerTrackFit>(x, y, z);}
	virtual int runKernelImpl(classArgument<AliGPUTRDTrackerGPU>, const krnlExec& x, const krnlRunRange& y, const krnlEvent& z) {return T::template runKernelBackend<AliGPUTRDTrackerGPU>(x, y, z);}
};

class AliGPUReconstructionCPUBackend : public AliGPUReconstruction
{
public:
	virtual ~AliGPUReconstructionCPUBackend() = default;
	
protected:
	AliGPUReconstructionCPUBackend(const AliGPUCASettingsProcessing& cfg) : AliGPUReconstruction(cfg) {}
	template <class T, int I = 0, typename... Args> int runKernelBackend(const krnlExec& x, const krnlRunRange& y, const krnlEvent& z, const Args&... args)
	{
		if (x.device == krnlDeviceType::Device) throw std::runtime_error("Cannot run device kernel on host");
		unsigned int num = y.num == 0 || y.num == -1 ? 1 : y.num;
		for (unsigned int k = 0;k < num;k++)
		{
			for (unsigned int iB = 0; iB < x.nBlocks; iB++)
			{
				typename T::AliGPUTPCSharedMemory smem;
				T::template Thread<I>(x.nBlocks, 1, iB, 0, smem, T::Worker(*mHostConstantMem)[y.start + k], args...);
			}
		}
		return 0;
	}
};

class AliGPUReconstructionCPU : public AliGPUReconstructionImpl<AliGPUReconstructionCPUBackend>
{
	friend class AliGPUReconstruction;
	
public:
	virtual ~AliGPUReconstructionCPU() = default;
	
	template <class S, int I = 0, typename... Args> inline int runKernel(const krnlExec& x, HighResTimer* t = nullptr, const krnlRunRange& y = krnlRunRangeNone, const krnlEvent& z = krnlEvent(), const Args&... args)
	{
		if (mDeviceProcessingSettings.debugLevel >= 3) printf("Running %s Stream %d (Range %d/%d)\n", typeid(S).name(), x.stream, y.start, y.num);
		if (t && mDeviceProcessingSettings.debugLevel) t->Start();
		if (runKernelImpl(classArgument<S, I>(), x, y, z, args...)) return 1;
		if (mDeviceProcessingSettings.debugLevel)
		{
			if (GPUDebug(typeid(S).name(), x.stream)) throw std::runtime_error("kernel failure");
			if (t) t->Stop();
		}
		return 0;
	}
	
	virtual int RunTPCTrackingSlices();
	virtual int RunTPCTrackingMerger();
	virtual int RefitMergedTracks(bool resetTimers);
	virtual int RunTRDTracking();
	virtual int RunStandalone();
	
	virtual int GPUDebug(const char* state = "UNKNOWN", int stream = -1);
	
	void TransferMemoryResourceToGPU(AliGPUMemoryResource* res, int stream = -1, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int nEvents = 1) {TransferMemoryInternal(res, stream, ev, evList, nEvents, true, res->Ptr(), res->PtrDevice());}
	void TransferMemoryResourceToHost(AliGPUMemoryResource* res, int stream = -1, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int nEvents = 1) {TransferMemoryInternal(res, stream, ev, evList, nEvents, false, res->PtrDevice(), res->Ptr());}
	virtual void TransferMemoryInternal(AliGPUMemoryResource* res, int stream, deviceEvent* ev, deviceEvent* evList, int nEvents, bool toGPU, void* src, void* dst);
	virtual void WriteToConstantMemory(size_t offset, const void* src, size_t size, int stream = -1, deviceEvent* ev = nullptr);

	void TransferMemoryResourcesToGPU(AliGPUProcessor* proc, int stream = -1, bool all = false) {TransferMemoryResourcesHelper(proc, stream, all, true);}
	void TransferMemoryResourcesToHost(AliGPUProcessor* proc, int stream = -1, bool all = false) {TransferMemoryResourcesHelper(proc, stream, all, false);}
	void TransferMemoryResourceLinkToGPU(short res, int stream = -1, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int nEvents = 1) {TransferMemoryResourceToGPU(&mMemoryResources[res], stream, ev, evList, nEvents);}
	void TransferMemoryResourceLinkToHost(short res, int stream = -1, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int nEvents = 1) {TransferMemoryResourceToHost(&mMemoryResources[res], stream, ev, evList, nEvents);}
	
	HighResTimer timerTPCtracking[NSLICES][10];
	
protected:
	AliGPUReconstructionCPU(const AliGPUCASettingsProcessing& cfg) : AliGPUReconstructionImpl(cfg) {}

private:
	void TransferMemoryResourcesHelper(AliGPUProcessor* proc, int stream, bool all, bool toGPU);
};

#endif
