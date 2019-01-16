#ifndef ALIGPUPROCESSOR_H
#define ALIGPUPROCESSOR_H

#include "AliTPCCommonDef.h"
#include "AliGPUTPCDef.h"

#ifndef GPUCA_GPUCODE
#include <cstddef>
#endif

class AliGPUReconstruction;
MEM_CLASS_PRE() class AliGPUCAParam;

class AliGPUProcessor
{
	friend class AliGPUReconstruction;
	friend class AliGPUReconstructionDeviceBase;
	friend class AliGPUMemoryResource;
	
public:
	enum ProcessorType {PROCESSOR_TYPE_CPU = 0, PROCESSOR_TYPE_DEVICE = 1, PROCESSOR_TYPE_SLAVE = 2};

#ifndef GPUCA_GPUCODE
	AliGPUProcessor();
	~AliGPUProcessor();
	AliGPUProcessor(const AliGPUProcessor&) CON_DELETE;
	AliGPUProcessor& operator= (const AliGPUProcessor&) CON_DELETE;
	void InitGPUProcessor(AliGPUReconstruction* rec, ProcessorType type = PROCESSOR_TYPE_CPU, AliGPUProcessor* slaveProcessor = NULL);
#endif

	void* InputMemory() const;
	void* ScratchMemory() const;
	size_t InputMemorySize() const;
	size_t ScratchMemorySize() const;

protected:
	AliGPUReconstruction* mRec;
	ProcessorType mGPUProcessorType;
	AliGPUProcessor* mDeviceProcessor;
	GPUglobalref() const MEM_GLOBAL(AliGPUCAParam) *mParam;
	
	short mMemoryResInput;
	short mMemoryResOutput;
    short mMemoryResScratch;
    short mMemoryResScratchHost;
	short mMemoryResPermanent;
};

#endif
