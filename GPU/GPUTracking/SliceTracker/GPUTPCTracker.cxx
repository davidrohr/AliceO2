// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUTPCTracker.cxx
/// \author Sergey Gorbunov, Ivan Kisel, David Rohr

#include "GPUTPCTracker.h"
#include "GPUTPCRow.h"
#include "GPUTPCTrack.h"
#include "GPUCommonMath.h"

#include "GPUTPCClusterData.h"
#include "GPUTPCSliceOutput.h"
#include "GPUOutputControl.h"
#include "GPUTPCTrackletConstructor.h"
#include "GPUTPCTrackLinearisation.h"

#include "GPUTPCTrackParam.h"

#include "GPUTPCGPUConfig.h"

#if !defined(GPUCA_GPUCODE)
#include <string.h>
#include <cmath>
#include <algorithm>
#include <stdexcept>

#include "GPUReconstruction.h"
#endif

ClassImp( GPUTPCTracker )

#if !defined(GPUCA_GPUCODE)

GPUTPCTracker::GPUTPCTracker() :
	GPUProcessor(),
	fStageAtSync( nullptr ),
	fLinkTmpMemory( nullptr ),
	fISlice(-1),
	fData(),
	fNMaxStartHits( 0 ),
	fNMaxTracklets( 0 ),
	fNMaxTracks( 0 ),
	fNMaxTrackHits( 0 ),
	mMemoryResScratch( -1 ),
	mMemoryResScratchHost( -1 ),
	mMemoryResCommon( -1 ),
	mMemoryResTracklets( -1 ),
	mMemoryResTracks( -1 ),
	mMemoryResTrackHits( -1 ),
	fRowStartHitCountOffset( nullptr ),
	fTrackletTmpStartHits( nullptr ),
	fGPUTrackletTemp( nullptr ),
	fGPUParametersConst(),
	fCommonMem( nullptr ),
	fTrackletStartHits( nullptr ),
	fTracklets( nullptr ),
	fTrackletRowHits( nullptr ),
	fTracks( nullptr ),
	fTrackHits( nullptr ),
	fOutput( nullptr ),
	fOutputMemory(nullptr)
{}
	
GPUTPCTracker::~GPUTPCTracker()
{
	if (fOutputMemory) free(fOutputMemory);
}

// ----------------------------------------------------------------------------------
void GPUTPCTracker::SetSlice(int iSlice)
{
	fISlice = iSlice;
}
void GPUTPCTracker::InitializeProcessor()
{
	if (fISlice < 0) throw std::runtime_error("Slice not set");
	InitializeRows(mCAParam);
	SetupCommonMemory();
}

void* GPUTPCTracker::SetPointersScratch(void* mem)
{
	computePointerWithAlignment( mem, fTrackletStartHits, fNMaxStartHits);
	if (mRec->GetDeviceProcessingSettings().memoryAllocationStrategy != GPUMemoryResource::ALLOCATION_INDIVIDUAL)
	{
		mem = SetPointersTracklets(mem);
	}
	if (mRec->IsGPU())
	{
		computePointerWithAlignment(mem, fTrackletTmpStartHits, GPUCA_ROW_COUNT * GPUCA_MAX_ROWSTARTHITS);
		computePointerWithAlignment(mem, fRowStartHitCountOffset, GPUCA_ROW_COUNT);
	}
	return mem;
}

void* GPUTPCTracker::SetPointersScratchHost(void* mem)
{
	computePointerWithAlignment(mem, fLinkTmpMemory, mRec->Res(fData.MemoryResScratch()).Size());
	return mem;
}

void* GPUTPCTracker::SetPointersCommon(void* mem)
{
	computePointerWithAlignment(mem, fCommonMem, 1);
	return mem;
}

void GPUTPCTracker::RegisterMemoryAllocation()
{
	mMemoryResScratch = mRec->RegisterMemoryAllocation(this, &GPUTPCTracker::SetPointersScratch, GPUMemoryResource::MEMORY_SCRATCH, "TrackerScratch");
	mMemoryResScratchHost = mRec->RegisterMemoryAllocation(this, &GPUTPCTracker::SetPointersScratchHost, GPUMemoryResource::MEMORY_SCRATCH_HOST, "TrackerHost");
	mMemoryResCommon = mRec->RegisterMemoryAllocation(this, &GPUTPCTracker::SetPointersCommon, GPUMemoryResource::MEMORY_PERMANENT, "TrackerCommon");
	
	auto type = GPUMemoryResource::MEMORY_OUTPUT;
	if (mRec->GetDeviceProcessingSettings().memoryAllocationStrategy == GPUMemoryResource::ALLOCATION_INDIVIDUAL)
	{	//For individual scheme, we allocate tracklets separately, and change the type for the following allocations to custom
		type = GPUMemoryResource::MEMORY_CUSTOM;
		mMemoryResTracklets = mRec->RegisterMemoryAllocation(this, &GPUTPCTracker::SetPointersTracklets, type, "TrackerTracklets");
	}
	mMemoryResTracks = mRec->RegisterMemoryAllocation(this, &GPUTPCTracker::SetPointersTracks, type, "TrackerTracks");
	mMemoryResTrackHits = mRec->RegisterMemoryAllocation(this, &GPUTPCTracker::SetPointersTrackHits, type, "TrackerTrackHits");
}

GPUhd() void* GPUTPCTracker::SetPointersTracklets(void* mem)
{
	computePointerWithAlignment(mem, fTracklets, fNMaxTracklets);
#ifdef EXTERN_ROW_HITS
	computePointerWithAlignment(mem, fTrackletRowHits, fNMaxTracklets * GPUCA_ROW_COUNT);
#endif
	return mem;
}

GPUhd() void* GPUTPCTracker::SetPointersTracks(void* mem)
{
	computePointerWithAlignment(mem, fTracks, fNMaxTracks);
	return mem;
}

GPUhd() void* GPUTPCTracker::SetPointersTrackHits(void* mem)
{
	computePointerWithAlignment(mem, fTrackHits, fNMaxTrackHits);
	return mem;
}

void GPUTPCTracker::SetMaxData()
{
	fNMaxStartHits = fData.NumberOfHits();
	fNMaxTracklets = GPUCA_MAX_TRACKLETS;
	fNMaxTracks = GPUCA_MAX_TRACKS;
	fNMaxTrackHits = fData.NumberOfHits() + 1000;
	if (mRec->IsGPU())
	{
		if (fNMaxStartHits > GPUCA_MAX_ROWSTARTHITS * GPUCA_ROW_COUNT) fNMaxStartHits = GPUCA_MAX_ROWSTARTHITS * GPUCA_ROW_COUNT;
	}
}

void GPUTPCTracker::UpdateMaxData()
{
	fNMaxTracklets = fCommonMem->fNTracklets * 2;
	fNMaxTracks = fCommonMem->fNTracklets * 2 + 50;
	fNMaxTrackHits = fNMaxStartHits * 2;
}

void GPUTPCTracker::SetupCommonMemory()
{
	new(fCommonMem) commonMemoryStruct;
}

int GPUTPCTracker::ReadEvent()
{
	SetupCommonMemory();

	//* Convert input hits, create grids, etc.
	if (fData.InitFromClusterData())
	{
		printf("Error initializing from cluster data\n");
		return 1;
	}
	if (fData.MaxZ() > 300 && !mCAParam->ContinuousTracking)
	{
		printf("Need to set continuous tracking mode for data outside of the TPC volume!\n");
		return 1;
	}
	if (mRec->GetDeviceProcessingSettings().memoryAllocationStrategy == GPUMemoryResource::ALLOCATION_INDIVIDUAL)
	{
		fNMaxStartHits = fData.NumberOfHits();
	}
	return 0;
}

GPUh() int GPUTPCTracker::CheckEmptySlice()
{
	//Check if the Slice is empty, if so set the output apropriate and tell the reconstuct procesdure to terminate
	if ( NHitsTotal() < 1 )
	{
		fCommonMem->fNTracks = fCommonMem->fNTrackHits = 0;
		WriteOutputPrepare();
		fOutput->SetNTracks( 0 );
		fOutput->SetNTrackClusters( 0 );
		return 1;
	}
	return 0;
}

GPUh() void GPUTPCTracker::WriteOutputPrepare()
{
	GPUTPCSliceOutput::Allocate(fOutput, fCommonMem->fNTracks, fCommonMem->fNTrackHits, &mRec->OutputControl(), fOutputMemory);
}

template <class T> static inline bool SortComparison(const T& a, const T& b)
{
	return(a.fSortVal < b.fSortVal);
}

GPUh() void GPUTPCTracker::WriteOutput()
{
	fOutput->SetNTracks( 0 );
	fOutput->SetNLocalTracks( 0 );
	fOutput->SetNTrackClusters( 0 );
	
	if (fCommonMem->fNTracks == 0) return;
	if (fCommonMem->fNTracks > MAX_SLICE_NTRACK)
	{
		printf("Maximum number of tracks exceeded, cannot store\n");
		return;
	}

	int nStoredHits = 0;
	int nStoredTracks = 0;
	int nStoredLocalTracks = 0;

	GPUTPCSliceOutTrack *out = fOutput->FirstTrack();
	
	trackSortData* trackOrder = new trackSortData[fCommonMem->fNTracks];
	for (int i = 0;i < fCommonMem->fNTracks;i++)
	{
		trackOrder[i].fTtrack = i;
		trackOrder[i].fSortVal = fTracks[trackOrder[i].fTtrack].NHits() / 1000.f + fTracks[trackOrder[i].fTtrack].Param().GetZ() * 100.f + fTracks[trackOrder[i].fTtrack].Param().GetY();
	}
	std::sort(trackOrder, trackOrder + fCommonMem->fNLocalTracks, SortComparison<trackSortData>);
	std::sort(trackOrder + fCommonMem->fNLocalTracks, trackOrder + fCommonMem->fNTracks, SortComparison<trackSortData>);
	
	for (int iTrTmp = 0;iTrTmp < fCommonMem->fNTracks;iTrTmp++)
	{
		const int iTr = trackOrder[iTrTmp].fTtrack;
		GPUTPCTrack &iTrack = fTracks[iTr];

		out->SetParam( iTrack.Param() );
		out->SetLocalTrackId( iTrack.LocalTrackId() );
		int nClu = 0;
		int iID = iTrack.FirstHitID();

		for (int ith = 0;ith < iTrack.NHits();ith++)
		{
			const GPUTPCHitId &ic = fTrackHits[iID + ith];
			int iRow = ic.RowIndex();
			int ih = ic.HitIndex();

			const GPUTPCRow &row = fData.Row( iRow );
#ifdef GPUCA_ARRAY_BOUNDS_CHECKS
			if (ih >= row.NHits() || ih < 0)
			{
				printf("Array out of bounds access (Sector Row) (Hit %d / %d - NumC %d): Sector %d Row %d Index %d\n", ith, iTrack.NHits(), NHitsTotal(), fISlice, iRow, ih);
				fflush(stdout);
				continue;
			}
#endif
			int clusterIndex = fData.ClusterDataIndex( row, ih );

#ifdef GPUCA_ARRAY_BOUNDS_CHECKS
			if (clusterIndex >= NHitsTotal() || clusterIndex < 0)
			{
				printf("Array out of bounds access (Cluster Data) (Hit %d / %d - NumC %d): Sector %d Row %d Hit %d, Clusterdata Index %d\n", ith, iTrack.NHits(), NHitsTotal(), fISlice, iRow, ih, clusterIndex);
				fflush(stdout);
				continue;
			}
#endif

			float origX = fData.ClusterData()[clusterIndex].fX;
			float origY = fData.ClusterData()[clusterIndex].fY;
			float origZ = fData.ClusterData()[clusterIndex].fZ;
			int id = fData.ClusterData()[clusterIndex].fId;
			unsigned char flags = fData.ClusterData()[clusterIndex].fFlags;
			unsigned short amp = fData.ClusterData()[clusterIndex].fAmp;
			GPUTPCSliceOutCluster c;
			c.Set( id, iRow, flags, amp, origX, origY, origZ );
#ifdef GMPropagatePadRowTime
			c.fPad = fData.ClusterData()[clusterIndex].fPad;
			c.fTime = fData.ClusterData()[clusterIndex].fTime;
#endif
			out->SetCluster( nClu, c );
			nClu++;
		}

		nStoredTracks++;
		if (iTr < fCommonMem->fNLocalTracks) nStoredLocalTracks++;
		nStoredHits+=nClu;
		out->SetNClusters( nClu );
		out = out->NextTrack();
	}
	delete[] trackOrder;

	fOutput->SetNTracks( nStoredTracks );
	fOutput->SetNLocalTracks( nStoredLocalTracks );
	fOutput->SetNTrackClusters( nStoredHits );
	if (mCAParam->debugLevel >= 3) printf("Slice %d, Output: Tracks %d, local tracks %d, hits %d\n", fISlice, nStoredTracks, nStoredLocalTracks, nStoredHits);
}

GPUh() int GPUTPCTracker::PerformGlobalTrackingRun(GPUTPCTracker& sliceNeighbour, int iTrack, int rowIndex, float angle, int direction)
{
	/*for (int j = 0;j < fTracks[j].NHits();j++)
	{
		printf("Hit %3d: Row %3d: X %3.7lf Y %3.7lf\n", j, fTrackHits[fTracks[iTrack].FirstHitID() + j].RowIndex(), Row(fTrackHits[fTracks[iTrack].FirstHitID() + j].RowIndex()).X(),
		(float) Data().HitDataY(Row(fTrackHits[fTracks[iTrack].FirstHitID() + j].RowIndex()), fTrackHits[fTracks[iTrack].FirstHitID() + j].HitIndex()) * Row(fTrackHits[fTracks[iTrack].FirstHitID() + j].RowIndex()).HstepY() + Row(fTrackHits[fTracks[iTrack].FirstHitID() + j].RowIndex()).Grid().YMin());
	}*/

	if (sliceNeighbour.fCommonMem->fNTracklets == 0) return(0);

	GPUTPCTrackParam tParam;
	tParam.InitParam();
	tParam.SetCov( 0, 0.05 );
	tParam.SetCov( 2, 0.05 );
	tParam.SetCov( 5, 0.001 );
	tParam.SetCov( 9, 0.001 );
	tParam.SetCov( 14, 0.05 );
	tParam.SetParam(fTracks[iTrack].Param());

	//printf("Parameters X %f Y %f Z %f SinPhi %f DzDs %f QPt %f SignCosPhi %f\n", tParam.X(), tParam.Y(), tParam.Z(), tParam.SinPhi(), tParam.DzDs(), tParam.QPt(), tParam.SignCosPhi());
	if (!tParam.Rotate(angle, GPUCA_MAX_SIN_PHI)) return(0);
	//printf("Rotated X %f Y %f Z %f SinPhi %f DzDs %f QPt %f SignCosPhi %f\n", tParam.X(), tParam.Y(), tParam.Z(), tParam.SinPhi(), tParam.DzDs(), tParam.QPt(), tParam.SignCosPhi());

	int maxRowGap = 10;
	GPUTPCTrackLinearisation t0( tParam );
	do
	{
		rowIndex += direction;
		if (!tParam.TransportToX(sliceNeighbour.Row(rowIndex).X(), t0, mCAParam->ConstBz, GPUCA_MAX_SIN_PHI)) return(0); //Reuse t0 linearization until we are in the next sector
		//printf("Transported X %f Y %f Z %f SinPhi %f DzDs %f QPt %f SignCosPhi %f (MaxY %f)\n", tParam.X(), tParam.Y(), tParam.Z(), tParam.SinPhi(), tParam.DzDs(), tParam.QPt(), tParam.SignCosPhi(), sliceNeighbour.Row(rowIndex).MaxY());
		if (--maxRowGap == 0) return(0);
	} while (fabsf(tParam.Y()) > sliceNeighbour.Row(rowIndex).MaxY());

	float err2Y, err2Z;
	GetErrors2( rowIndex, tParam.Z(), tParam.SinPhi(), tParam.DzDs(), err2Y, err2Z );
	if (tParam.GetCov(0) < err2Y) tParam.SetCov(0, err2Y);
	if (tParam.GetCov(2) < err2Z) tParam.SetCov(2, err2Z);

	int nHits = GPUTPCTrackletConstructor::GPUTPCTrackletConstructorGlobalTracking(sliceNeighbour, tParam, rowIndex, direction, 0);
	if (nHits >= GLOBAL_TRACKING_MIN_HITS)
	{
		//printf("%d hits found\n", nHits);
		int trackId = CAMath::AtomicAdd(&sliceNeighbour.fCommonMem->fNTracks, 1);
		int hitId = CAMath::AtomicAdd(&sliceNeighbour.fCommonMem->fNTrackHits, nHits);
		if (direction == 1)
		{
			int i = 0;
			while (i < nHits)
			{
#ifdef EXTERN_ROW_HITS
				const calink rowHit = sliceNeighbour.TrackletRowHits()[rowIndex * *sliceNeighbour.NTracklets()];
#else
				const calink rowHit = sliceNeighbour.Tracklet(0).RowHit(rowIndex);
#endif
				if (rowHit != CALINK_INVAL)
				{
					//printf("New track: entry %d, row %d, hitindex %d\n", i, rowIndex, sliceNeighbour.fTrackletRowHits[rowIndex * sliceNeighbour.fCommonMem->fNTracklets]);
					sliceNeighbour.fTrackHits[hitId + i].Set(rowIndex, rowHit);
					//if (i == 0) tParam.TransportToX(sliceNeighbour.Row(rowIndex).X(), mCAParam->ConstBz(), GPUCA_MAX_SIN_PHI); //Use transport with new linearisation, we have changed the track in between - NOT needed, fitting will always start at outer end of global track!
					i++;
				}
				rowIndex++;
			}
		}
		else
		{
			int i = nHits - 1;
			while (i >= 0)
			{
#ifdef EXTERN_ROW_HITS
				const calink rowHit = sliceNeighbour.TrackletRowHits()[rowIndex * *sliceNeighbour.NTracklets()];
#else
				const calink rowHit = sliceNeighbour.Tracklet(0).RowHit(rowIndex);
#endif
				if (rowHit != CALINK_INVAL)
				{
					//printf("New track: entry %d, row %d, hitindex %d\n", i, rowIndex, sliceNeighbour.fTrackletRowHits[rowIndex * sliceNeighbour.fCommonMem->fNTracklets]);
					sliceNeighbour.fTrackHits[hitId + i].Set(rowIndex, rowHit);
					i--;
				}
				rowIndex--;
			}
		}
		GPUTPCTrack& track = sliceNeighbour.fTracks[trackId];
		track.SetAlive(1);
		track.SetParam(tParam.GetParam());
		track.SetNHits(nHits);
		track.SetFirstHitID(hitId);
		track.SetLocalTrackId((fISlice << 24) | fTracks[iTrack].LocalTrackId());
	}

	return(nHits >= GLOBAL_TRACKING_MIN_HITS);
}

GPUh() void GPUTPCTracker::PerformGlobalTracking(GPUTPCTracker& sliceLeft, GPUTPCTracker& sliceRight, int MaxTracksLeft, int MaxTracksRight)
{
	int ul = 0, ur = 0, ll = 0, lr = 0;
	
	int nTrkLeft = sliceLeft.fCommonMem->fNTracklets, nTrkRight = sliceRight.fCommonMem->fNTracklets;
	sliceLeft.fCommonMem->fNTracklets = sliceRight.fCommonMem->fNTracklets = 1;
	GPUTPCTracklet *trkLeft = sliceLeft.fTracklets, *trkRight = sliceRight.fTracklets;
	sliceLeft.fTracklets = sliceRight.fTracklets = new GPUTPCTracklet;
#ifdef EXTERN_ROW_HITS
	calink *lnkLeft = sliceLeft.fTrackletRowHits, *lnkRight = sliceRight.fTrackletRowHits;
	sliceLeft.fTrackletRowHits = sliceRight.fTrackletRowHits = new calink[GPUCA_ROW_COUNT];
#endif

	for (int i = 0;i < fCommonMem->fNLocalTracks;i++)
	{
		{
			const int tmpHit = fTracks[i].FirstHitID();
			if (fTrackHits[tmpHit].RowIndex() >= GLOBAL_TRACKING_MIN_ROWS && fTrackHits[tmpHit].RowIndex() < GLOBAL_TRACKING_RANGE)
			{
				int rowIndex = fTrackHits[tmpHit].RowIndex();
				const GPUTPCRow& row = Row(rowIndex);
				float Y = (float) Data().HitDataY(row, fTrackHits[tmpHit].HitIndex()) * row.HstepY() + row.Grid().YMin();
				if (sliceLeft.NHitsTotal() < 1) {}
				else if (sliceLeft.fCommonMem->fNTracks >= MaxTracksLeft) {printf("Insufficient memory for global tracking (%d:l %d / %d)\n", fISlice, sliceLeft.fCommonMem->fNTracks, MaxTracksLeft);}
				else if (Y < -row.MaxY() * GLOBAL_TRACKING_Y_RANGE_LOWER_LEFT)
				{
					//printf("Track %d, lower row %d, left border (%f of %f)\n", i, fTrackHits[tmpHit].RowIndex(), Y, -row.MaxY());
					ll += PerformGlobalTrackingRun(sliceLeft, i, rowIndex, -mCAParam->DAlpha, -1);
				}
				if (sliceRight.NHitsTotal() < 1) {}
				else if (sliceRight.fCommonMem->fNTracks >= MaxTracksRight) {printf("Insufficient memory for global tracking (%d:r %d / %d)\n", fISlice, sliceRight.fCommonMem->fNTracks, MaxTracksRight);}
				else if (Y > row.MaxY() * GLOBAL_TRACKING_Y_RANGE_LOWER_RIGHT)
				{
					//printf("Track %d, lower row %d, right border (%f of %f)\n", i, fTrackHits[tmpHit].RowIndex(), Y, row.MaxY());
					lr += PerformGlobalTrackingRun(sliceRight, i, rowIndex, mCAParam->DAlpha, -1);
				}
			}
		}
		
		{
			const int tmpHit = fTracks[i].FirstHitID() + fTracks[i].NHits() - 1;
			if (fTrackHits[tmpHit].RowIndex() < GPUCA_ROW_COUNT - GLOBAL_TRACKING_MIN_ROWS && fTrackHits[tmpHit].RowIndex() >= GPUCA_ROW_COUNT - GLOBAL_TRACKING_RANGE)
			{
				int rowIndex = fTrackHits[tmpHit].RowIndex();
				const GPUTPCRow& row = Row(rowIndex);
				float Y = (float) Data().HitDataY(row, fTrackHits[tmpHit].HitIndex()) * row.HstepY() + row.Grid().YMin();
				if (sliceLeft.NHitsTotal() < 1) {}
				else if (sliceLeft.fCommonMem->fNTracks >= MaxTracksLeft) {printf("Insufficient memory for global tracking (%d:l %d / %d)\n", fISlice, sliceLeft.fCommonMem->fNTracks, MaxTracksLeft);}
				else if (Y < -row.MaxY() * GLOBAL_TRACKING_Y_RANGE_UPPER_LEFT)
				{
					//printf("Track %d, upper row %d, left border (%f of %f)\n", i, fTrackHits[tmpHit].RowIndex(), Y, -row.MaxY());
					ul += PerformGlobalTrackingRun(sliceLeft, i, rowIndex, -mCAParam->DAlpha, 1);
				}
				if (sliceRight.NHitsTotal() < 1) {}
				else if (sliceRight.fCommonMem->fNTracks >= MaxTracksRight) {printf("Insufficient memory for global tracking (%d:r %d / %d)\n", fISlice, sliceRight.fCommonMem->fNTracks, MaxTracksRight);}
				else if (Y > row.MaxY() * GLOBAL_TRACKING_Y_RANGE_UPPER_RIGHT)
				{
					//printf("Track %d, upper row %d, right border (%f of %f)\n", i, fTrackHits[tmpHit].RowIndex(), Y, row.MaxY());
					ur += PerformGlobalTrackingRun(sliceRight, i, rowIndex, mCAParam->DAlpha, 1);
				}
			}
		}
	}
	
	sliceLeft.fCommonMem->fNTracklets = nTrkLeft;sliceRight.fCommonMem->fNTracklets = nTrkRight;
	delete sliceLeft.fTracklets;
	sliceLeft.fTracklets = trkLeft;sliceRight.fTracklets = trkRight;
#ifdef EXTERN_ROW_HITS
	delete[] sliceLeft.fTrackletRowHits;
	sliceLeft.fTrackletRowHits = lnkLeft;sliceRight.fTrackletRowHits = lnkRight;
#endif
	//printf("Global Tracking Result: Slide %2d: LL %3d LR %3d UL %3d UR %3d\n", mCAParam->ISlice(), ll, lr, ul, ur);
}

#endif
