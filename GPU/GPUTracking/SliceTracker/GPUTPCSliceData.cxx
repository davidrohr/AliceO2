// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUTPCSliceData.cxx
/// \author Matthias Kretz, Sergey Gorbunov, David Rohr

#include "GPUParam.h"
#include "GPUTPCClusterData.h"
#include "GPUTPCGPUConfig.h"
#include "GPUTPCHit.h"
#include "GPUTPCSliceData.h"
#include "GPUReconstruction.h"
#include <iostream>
#include <string.h>
#include "utils/vecpod.h"

// calculates an approximation for 1/sqrt(x)
// Google for 0x5f3759df :)
static inline float fastInvSqrt(float _x)
{
	// the function calculates fast inverse sqrt
	union
	{
		float f;
		int i;
	} x = {_x};
	const float xhalf = 0.5f * x.f;
	x.i = 0x5f3759df - (x.i >> 1);
	x.f = x.f * (1.5f - xhalf * x.f * x.f);
	return x.f;
}

inline void GPUTPCSliceData::CreateGrid(GPUTPCRow *row, const float2 *data, int ClusterDataHitNumberOffset)
{
	// grid creation
	if (row->NHits() <= 0)
	{ // no hits or invalid data
		// grid coordinates don't matter, since there are no hits
		row->fGrid.CreateEmpty();
		return;
	}

	float yMin = 1.e3f;
	float yMax = -1.e3f;
	float zMin = 1.e3f;
	float zMax = -1.e3f;
	for (int i = ClusterDataHitNumberOffset; i < ClusterDataHitNumberOffset + row->fNHits; ++i)
	{
		const float y = data[i].x;
		const float z = data[i].y;
		if (yMax < y) yMax = y;
		if (yMin > y) yMin = y;
		if (zMax < z) zMax = z;
		if (zMin > z) zMin = z;
	}

	float dz = zMax - zMin;
	float tfFactor = 1.;
	if (dz > 270.)
	{
		tfFactor = dz / 250.;
		dz = 250.;
	}
	const float norm = fastInvSqrt(row->fNHits / tfFactor);
	row->fGrid.Create(yMin, yMax, zMin, zMax,
	                  CAMath::Max((yMax - yMin) * norm, 2.f),
	                  CAMath::Max(dz * norm, 2.f));
}

inline int GPUTPCSliceData::PackHitData(GPUTPCRow *const row, const GPUTPCHit* binSortedHits)
{
	// hit data packing
	static const float maxVal = (((long long int) 1 << CAMath::Min((size_t) 24, sizeof(cahit) * 8)) - 1); //Stay within float precision in any case!
	static const float packingConstant = 1.f / (maxVal - 2.);
	const float y0 = row->fGrid.YMin();
	const float z0 = row->fGrid.ZMin();
	const float stepY = (row->fGrid.YMax() - y0) * packingConstant;
	const float stepZ = (row->fGrid.ZMax() - z0) * packingConstant;
	const float stepYi = 1.f / stepY;
	const float stepZi = 1.f / stepZ;

	row->fHy0 = y0;
	row->fHz0 = z0;
	row->fHstepY = stepY;
	row->fHstepZ = stepZ;
	row->fHstepYi = stepYi;
	row->fHstepZi = stepZi;

	for (int hitIndex = 0; hitIndex < row->fNHits; ++hitIndex)
	{
		// bin sorted index!
		const int globalHitIndex = row->fHitNumberOffset + hitIndex;
		const GPUTPCHit &hh = binSortedHits[hitIndex];
		const float xx = ((hh.Y() - y0) * stepYi) + .5;
		const float yy = ((hh.Z() - z0) * stepZi) + .5;
		if (xx < 0 || yy < 0 || xx > maxVal || yy > maxVal)
		{
			std::cout << "!!!! hit packing error!!! " << xx << " " << yy << " (" << maxVal << ")" << std::endl;
			return 1;
		}
		// HitData is bin sorted
		fHitData[globalHitIndex].x = (cahit) xx;
		fHitData[globalHitIndex].y = (cahit) yy;
	}
	return 0;
}

void GPUTPCSliceData::InitializeProcessor()
{}
	
void GPUTPCSliceData::InitializeRows(const GPUParam &p)
{
	// initialisation of rows
	for (int i = 0; i < GPUCA_ROW_COUNT + 1; ++i)
	{
		new(&fRows[i]) GPUTPCRow;
	}
	for (int i = 0; i < GPUCA_ROW_COUNT; ++i)
	{
		fRows[i].fX = p.RowX[i];
		fRows[i].fMaxY = CAMath::Tan(p.DAlpha / 2.) * fRows[i].fX;
	}
}

void GPUTPCSliceData::SetClusterData(const GPUTPCClusterData *data, int nClusters, int clusterIdOffset)
{
	fClusterData = data;
	fNumberOfHits = nClusters;
	fClusterIdOffset = clusterIdOffset;
}

void GPUTPCSliceData::SetMaxData()
{
	int hitMemCount = GPUCA_ROW_COUNT * sizeof(GPUCA_ROWALIGNMENT) + fNumberOfHits;
	const unsigned int kVectorAlignment = 256;
	fNumberOfHitsPlusAlign = nextMultipleOf<(kVectorAlignment > sizeof(GPUCA_ROWALIGNMENT) ? kVectorAlignment : sizeof(GPUCA_ROWALIGNMENT)) / sizeof(int)>(hitMemCount);
}

void* GPUTPCSliceData::SetPointersInput(void* mem)
{
	const int firstHitInBinSize = (23 + sizeof(GPUCA_ROWALIGNMENT) / sizeof(int)) * GPUCA_ROW_COUNT + 4 * fNumberOfHits + 3;
	computePointerWithAlignment(mem, fHitData, fNumberOfHitsPlusAlign);
	computePointerWithAlignment(mem, fFirstHitInBin, firstHitInBinSize);
	if (mRec->GetRecoStepsGPU() & GPUReconstruction::RecoStep::TPCMerging)
	{
		mem = SetPointersScratchHost(mem);
	}
	return mem;
}

void* GPUTPCSliceData::SetPointersScratch(void* mem)
{
	computePointerWithAlignment(mem, fLinkUpData, fNumberOfHitsPlusAlign);
	computePointerWithAlignment(mem, fLinkDownData, fNumberOfHitsPlusAlign);
	computePointerWithAlignment(mem, fHitWeights, fNumberOfHitsPlusAlign + 16 / sizeof(*fHitWeights));
	return mem;
}

void* GPUTPCSliceData::SetPointersScratchHost(void* mem)
{
	computePointerWithAlignment(mem, fClusterDataIndex, fNumberOfHitsPlusAlign);
	return mem;
}

void* GPUTPCSliceData::SetPointersRows(void* mem)
{
	computePointerWithAlignment(mem, fRows, GPUCA_ROW_COUNT + 1);
	return mem;
}

void GPUTPCSliceData::RegisterMemoryAllocation()
{
	mMemoryResInput = mRec->RegisterMemoryAllocation(this, &GPUTPCSliceData::SetPointersInput, GPUMemoryResource::MEMORY_INPUT, "SliceInput");
	mMemoryResScratch = mRec->RegisterMemoryAllocation(this, &GPUTPCSliceData::SetPointersScratch, GPUMemoryResource::MEMORY_SCRATCH, "SliceLinks");
	if (!(mRec->GetRecoStepsGPU() & GPUReconstruction::RecoStep::TPCMerging)) mMemoryResScratchHost = mRec->RegisterMemoryAllocation(this, &GPUTPCSliceData::SetPointersScratchHost, GPUMemoryResource::MEMORY_SCRATCH_HOST, "SliceIds");
	mMemoryResRows = mRec->RegisterMemoryAllocation(this, &GPUTPCSliceData::SetPointersRows, GPUMemoryResource::MEMORY_PERMANENT, "SliceRows");
}

int GPUTPCSliceData::InitFromClusterData()
{
	////////////////////////////////////
	// 0. sort rows
	////////////////////////////////////
	
	fMaxZ = 0.f;

	float2 *YZData = new float2[fNumberOfHits];
	int *tmpHitIndex = new int[fNumberOfHits];

	int RowOffset[GPUCA_ROW_COUNT];
	int NumberOfClustersInRow[GPUCA_ROW_COUNT];
	memset(NumberOfClustersInRow, 0, GPUCA_ROW_COUNT * sizeof(NumberOfClustersInRow[0]));
	fFirstRow = GPUCA_ROW_COUNT;
	fLastRow = 0;

	for (int i = 0; i < fNumberOfHits; i++)
	{
		const int tmpRow = fClusterData[i].fRow;
		NumberOfClustersInRow[tmpRow]++;
		if (tmpRow > fLastRow) fLastRow = tmpRow;
		if (tmpRow < fFirstRow) fFirstRow = tmpRow;
	}
	int tmpOffset = 0;
	for (int i = fFirstRow; i <= fLastRow; i++)
	{
		if ((long long int) NumberOfClustersInRow[i] >= ((long long int) 1 << (sizeof(calink) * 8)))
		{
			printf("Too many clusters in row %d for row indexing (%d >= %lld), indexing insufficient\n", i, NumberOfClustersInRow[i], ((long long int) 1 << (sizeof(calink) * 8)));
			return (1);
		}
		if (NumberOfClustersInRow[i] >= (1 << 24))
		{
			printf("Too many clusters in row %d for hit id indexing (%d >= %d), indexing insufficient\n", i, NumberOfClustersInRow[i], 1 << 24);
			return (1);
		}
		RowOffset[i] = tmpOffset;
		tmpOffset += NumberOfClustersInRow[i];
	}

	{
		int RowsFilled[GPUCA_ROW_COUNT];
		memset(RowsFilled, 0, GPUCA_ROW_COUNT * sizeof(int));
		for (int i = 0; i < fNumberOfHits; i++)
		{
			float2 tmp;
			tmp.x = fClusterData[i].fY;
			tmp.y = fClusterData[i].fZ;
			if (fabsf(tmp.y) > fMaxZ) fMaxZ = fabsf(tmp.y);
			int tmpRow = fClusterData[i].fRow;
			int newIndex = RowOffset[tmpRow] + (RowsFilled[tmpRow])++;
			YZData[newIndex] = tmp;
			tmpHitIndex[newIndex] = i;
		}
	}
	if (fFirstRow == GPUCA_ROW_COUNT) fFirstRow = 0;

	////////////////////////////////////
	// 2. fill HitData and FirstHitInBin
	////////////////////////////////////

	const int numberOfRows = fLastRow - fFirstRow + 1;
	for (int rowIndex = 0; rowIndex < fFirstRow; ++rowIndex)
	{
		GPUTPCRow &row = fRows[rowIndex];
		row.fGrid.CreateEmpty();
		row.fNHits = 0;
		row.fFullSize = 0;
		row.fHitNumberOffset = 0;
		row.fFirstHitInBinOffset = 0;

		row.fHy0 = 0.f;
		row.fHz0 = 0.f;
		row.fHstepY = 1.f;
		row.fHstepZ = 1.f;
		row.fHstepYi = 1.f;
		row.fHstepZi = 1.f;
	}
	for (int rowIndex = fLastRow + 1; rowIndex < GPUCA_ROW_COUNT + 1; ++rowIndex)
	{
		GPUTPCRow &row = fRows[rowIndex];
		row.fGrid.CreateEmpty();
		row.fNHits = 0;
		row.fFullSize = 0;
		row.fHitNumberOffset = 0;
		row.fFirstHitInBinOffset = 0;

		row.fHy0 = 0.f;
		row.fHz0 = 0.f;
		row.fHstepY = 1.f;
		row.fHstepZ = 1.f;
		row.fHstepYi = 1.f;
		row.fHstepZi = 1.f;
	}

	vecpod<GPUTPCHit> binSortedHits(fNumberOfHits + sizeof(GPUCA_ROWALIGNMENT));

	int gridContentOffset = 0;
	int hitOffset = 0;

	int binCreationMemorySize = 103 * 2 + fNumberOfHits;
	vecpod<calink> binCreationMemory(binCreationMemorySize);

	for (int rowIndex = fFirstRow; rowIndex <= fLastRow; ++rowIndex)
	{
		GPUTPCRow &row = fRows[rowIndex];
		row.fNHits = NumberOfClustersInRow[rowIndex];
		row.fHitNumberOffset = hitOffset;
		hitOffset += nextMultipleOf<sizeof(GPUCA_ROWALIGNMENT) / sizeof(unsigned short)>(NumberOfClustersInRow[rowIndex]);

		row.fFirstHitInBinOffset = gridContentOffset;

		CreateGrid(&row, YZData, RowOffset[rowIndex]);
		const GPUTPCGrid &grid = row.fGrid;
		const int numberOfBins = grid.N();
		if ((long long int) numberOfBins >= ((long long int) 1 << (sizeof(calink) * 8)))
		{
			printf("Too many bins in row %d for grid (%d >= %lld), indexing insufficient\n", rowIndex, numberOfBins, ((long long int) 1 << (sizeof(calink) * 8)));
			delete[] YZData;
			delete[] tmpHitIndex;
			return (1);
		}

		int binCreationMemorySizeNew = numberOfBins * 2 + 6 + row.fNHits + sizeof(GPUCA_ROWALIGNMENT) / sizeof(unsigned short) * numberOfRows + 1;
		if (binCreationMemorySizeNew > binCreationMemorySize)
		{
			binCreationMemorySize = binCreationMemorySizeNew;
			binCreationMemory.resize(binCreationMemorySize);
		}

		calink* c = binCreationMemory.data();          // number of hits in all previous bins
		calink* bins = c + numberOfBins + 3;           // cache for the bin index for every hit in this row, 3 extra empty bins at the end!!!
		calink* filled = bins + row.fNHits;            // counts how many hits there are per bin

		for (unsigned int bin = 0; bin < row.fGrid.N() + 3; ++bin)
		{
			filled[bin] = 0; // initialize filled[] to 0
		}

		for (int hitIndex = 0; hitIndex < row.fNHits; ++hitIndex)
		{
			const int globalHitIndex = RowOffset[rowIndex] + hitIndex;
			const calink bin = row.fGrid.GetBin(YZData[globalHitIndex].x, YZData[globalHitIndex].y);

			bins[hitIndex] = bin;
			++filled[bin];
		}

		calink n = 0;
		for (int bin = 0; bin < numberOfBins + 3; ++bin)
		{
			c[bin] = n;
			n += filled[bin];
		}

		for (int hitIndex = 0; hitIndex < row.fNHits; ++hitIndex)
		{
			const calink bin = bins[hitIndex];
			--filled[bin];
			const calink ind = c[bin] + filled[bin]; // generate an index for this hit that is >= c[bin] and < c[bin + 1]
			const int globalBinsortedIndex = row.fHitNumberOffset + ind;
			const int globalHitIndex = RowOffset[rowIndex] + hitIndex;

			// allows to find the global hit index / coordinates from a global bin sorted hit index
			fClusterDataIndex[globalBinsortedIndex] = tmpHitIndex[globalHitIndex];
			binSortedHits[ind].SetY(YZData[globalHitIndex].x);
			binSortedHits[ind].SetZ(YZData[globalHitIndex].y);
		}

		if (PackHitData(&row, binSortedHits.data()))
		{
			delete[] YZData;
			delete[] tmpHitIndex;
			return (1);
		}

		for (int i = 0; i < numberOfBins; ++i)
		{
			fFirstHitInBin[row.fFirstHitInBinOffset + i] = c[i]; // global bin-sorted hit index
		}
		const calink a = c[numberOfBins];
		// grid.N is <= row.fNHits
		const int nn = numberOfBins + grid.Ny() + 3;
		for (int i = numberOfBins; i < nn; ++i)
		{
			fFirstHitInBin[row.fFirstHitInBinOffset + i] = a;
		}

		row.fFullSize = nn;
		gridContentOffset += nn;

		//Make pointer aligned
		gridContentOffset = nextMultipleOf<sizeof(GPUCA_ROWALIGNMENT) / sizeof(calink)>(gridContentOffset);
	}

	delete[] YZData;
	delete[] tmpHitIndex;

	return (0);
}
