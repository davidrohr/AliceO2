// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUTPCNeighboursFinder.cxx
/// \author Sergey Gorbunov, Ivan Kisel, David Rohr

#include "GPUTPCHit.h"
#include "GPUTPCHitArea.h"
#include "GPUTPCNeighboursFinder.h"
#include "GPUTPCTracker.h"
#include "GPUCommonMath.h"

template <> GPUd() void GPUTPCNeighboursFinder::Thread<0>(int /*nBlocks*/, int nThreads, int iBlock, int iThread, GPUsharedref() MEM_LOCAL(GPUTPCSharedMemory) &s, workerType &tracker)
{
	//* find neighbours

#ifdef GPUCA_GPUCODE
	for (unsigned int i = iThread; i < sizeof(MEM_PLAIN(GPUTPCRow)) / sizeof(int); i += nThreads)
	{
		reinterpret_cast<GPUsharedref() int *>(&s.fRow)[i] = reinterpret_cast<GPUglobalref() int *>(&tracker.SliceDataRows()[iBlock])[i];
		if (iBlock >= 2 && iBlock < GPUCA_ROW_COUNT - 2)
		{
			reinterpret_cast<GPUsharedref() int *>(&s.fRowUp)[i] = reinterpret_cast<GPUglobalref() int *>(&tracker.SliceDataRows()[iBlock + 2])[i];
			reinterpret_cast<GPUsharedref() int *>(&s.fRowDown)[i] = reinterpret_cast<GPUglobalref() int *>(&tracker.SliceDataRows()[iBlock - 2])[i];
		}
	}
	GPUbarrier();
#endif
	if (iThread == 0)
	{
		s.fIRow = iBlock;
		if (s.fIRow < GPUCA_ROW_COUNT)
		{
#ifdef GPUCA_GPUCODE
			GPUsharedref() const MEM_LOCAL(GPUTPCRow) &row = s.fRow;
#else
			GPUglobalref() const MEM_GLOBAL(GPUTPCRow) &row = tracker.Row(s.fIRow);
#endif
			s.fNHits = row.NHits();

			if ((s.fIRow >= 2) && (s.fIRow <= GPUCA_ROW_COUNT - 3))
			{
				s.fIRowUp = s.fIRow + 2;
				s.fIRowDn = s.fIRow - 2;

				// references to the rows above and below

#ifdef GPUCA_GPUCODE
				GPUsharedref() const MEM_LOCAL(GPUTPCRow) &rowUp = s.fRowUp;
				GPUsharedref() const MEM_LOCAL(GPUTPCRow) &rowDn = s.fRowDown;
#else
				GPUglobalref() const MEM_GLOBAL(GPUTPCRow) &rowUp = tracker.Row(s.fIRowUp);
				GPUglobalref() const MEM_GLOBAL(GPUTPCRow) &rowDn = tracker.Row(s.fIRowDn);
#endif
				// the axis perpendicular to the rows
				const float xDn = rowDn.X();
				const float x = row.X();
				const float xUp = rowUp.X();

				// number of hits in rows above and below
				s.fUpNHits = tracker.Row(s.fIRowUp).NHits();
				s.fDnNHits = tracker.Row(s.fIRowDn).NHits();

				// distance of the rows (absolute and relative)
				s.fUpDx = xUp - x;
				s.fDnDx = xDn - x;
				s.fUpTx = xUp / x;
				s.fDnTx = xDn / x;
				// UpTx/DnTx is used to move the HitArea such that central events are preferred (i.e. vertices
				// coming from y = 0, z = 0).

				//s.fGridUp = tracker.Row( s.fIRowUp ).Grid();
				//s.fGridDn = tracker.Row( s.fIRowDn ).Grid();
			}
		}
	}
    GPUbarrier();
    
	if (s.fIRow < GPUCA_ROW_COUNT)
	{
		if ((s.fIRow <= 1) || (s.fIRow >= GPUCA_ROW_COUNT - 2))
		{
#ifdef GPUCA_GPUCODE
			GPUsharedref() const MEM_LOCAL(GPUTPCRow) &row = s.fRow;
#else
			GPUglobalref() const MEM_GLOBAL(GPUTPCRow) &row = tracker.Row(s.fIRow);
#endif
			for (int ih = iThread; ih < s.fNHits; ih += nThreads)
			{
				tracker.SetHitLinkUpData(row, ih, CALINK_INVAL);
				tracker.SetHitLinkDownData(row, ih, CALINK_INVAL);
			}
		}
		else
		{
			/*#ifdef GPUCA_GPUCODE
      const GPUTPCRow &rowUp = s.fRowUp;
      const GPUTPCRow &rowDn = s.fRowDown;
#else
      const GPUTPCRow &rowUp = tracker.Row( s.fIRowUp );
      const GPUTPCRow &rowDn = tracker.Row( s.fIRowDn );
#endif

    for ( unsigned int ih = iThread; ih < s.fGridUp.N() + s.fGridUp.Ny() + 2; ih += nThreads ) {
      s.fGridContentUp[ih] = tracker.FirstHitInBin( rowUp, ih );
    }
    for ( unsigned int ih = iThread; ih < s.fGridDn.N() + s.fGridDn.Ny() + 2; ih += nThreads ) {
      s.fGridContentDn[ih] = tracker.FirstHitInBin( rowDn, ih );
    }*/
		}
	}
    GPUbarrier();

#ifdef GPUCA_GPUCODE
	if ((iBlock <= 1) || (iBlock >= GPUCA_ROW_COUNT - 2)) return;
#else
	if ((s.fIRow <= 1) || (s.fIRow >= GPUCA_ROW_COUNT - 2)) return;
#endif

	float chi2Cut = 3.f * 3.f * 4 * (s.fUpDx * s.fUpDx + s.fDnDx * s.fDnDx);
	//float chi2Cut = 3.*3.*(s.fUpDx*s.fUpDx + s.fDnDx*s.fDnDx ); //SG
#ifdef GPUCA_GPUCODE
	GPUsharedref() const MEM_LOCAL(GPUTPCRow) &row = s.fRow;
	GPUsharedref() const MEM_LOCAL(GPUTPCRow) &rowUp = s.fRowUp;
	GPUsharedref() const MEM_LOCAL(GPUTPCRow) &rowDn = s.fRowDown;
#else
	GPUglobalref() const MEM_GLOBAL(GPUTPCRow) &row = tracker.Row(s.fIRow);
	GPUglobalref() const MEM_GLOBAL(GPUTPCRow) &rowUp = tracker.Row(s.fIRowUp);
	GPUglobalref() const MEM_GLOBAL(GPUTPCRow) &rowDn = tracker.Row(s.fIRowDn);
#endif
	const float y0 = row.Grid().YMin();
	const float z0 = row.Grid().ZMin();
	const float stepY = row.HstepY();
	const float stepZ = row.HstepZ();

	for (int ih = iThread; ih < s.fNHits; ih += nThreads)
	{

		int linkUp = -1;
		int linkDn = -1;

		if (s.fDnNHits > 0 && s.fUpNHits > 0)
		{

			// coordinates of the hit in the current row
#if defined(GPUCA_TEXTURE_FETCH_NEIGHBORS)
			cahit2 tmpval = tex1Dfetch(gAliTexRefu2, ((char *) tracker.Data().HitData() - tracker.Data().GPUTextureBase()) / sizeof(cahit2) + row.HitNumberOffset() + ih);
			const float y = y0 + tmpval.x * stepY;
			const float z = z0 + tmpval.y * stepZ;
#else
			const float y = y0 + tracker.HitDataY(row, ih) * stepY;
			const float z = z0 + tracker.HitDataZ(row, ih) * stepZ;
#endif //GPUCA_TEXTURE_FETCH_NEIGHBORS

#if GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP > 0
#if defined(GPUCA_GPUCODE)
			GPUsharedref() calink *neighUp = s.fB[iThread];
			GPUsharedref() float2 *yzUp = s.fA[iThread];
#else
			GPUsharedref() calink *neighUp = s.fB;
			GPUsharedref() float2 *yzUp = s.fA;
#endif
#if defined(GPUCA_GPUCODE) & GPUCA_MAXN > GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP
			calink neighUp2[GPUCA_MAXN - GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP];
			float2 yzUp2[GPUCA_MAXN - GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP];
#endif
#else
			calink neighUp[GPUCA_MAXN];
			float2 yzUp[GPUCA_MAXN];
#endif //GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP > 0

			int nNeighUp = 0;
			GPUTPCHitArea areaDn, areaUp;

			const float kAngularMultiplier = tracker.Param().rec.SearchWindowDZDR;
			const float kAreaSize = tracker.Param().rec.NeighboursSearchArea;
			areaUp.Init(rowUp, tracker.Data(), y * s.fUpTx, kAngularMultiplier != 0.f ? z : (z * s.fUpTx), kAreaSize, kAngularMultiplier != 0.f ? (s.fUpDx * kAngularMultiplier) : kAreaSize);
			areaDn.Init(rowDn, tracker.Data(), y * s.fDnTx, kAngularMultiplier != 0.f ? z : (z * s.fDnTx), kAreaSize, kAngularMultiplier != 0.f ? (-s.fDnDx * kAngularMultiplier) : kAreaSize);

			do
			{
				GPUTPCHit h;
				int i = areaUp.GetNext(tracker, rowUp, tracker.Data(), &h);
				if (i < 0) break;
#if defined(GPUCA_GPUCODE) & GPUCA_MAXN > GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP & GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP > 0
				if (nNeighUp >= GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP)
				{
					neighUp2[nNeighUp - GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP] = (calink) i;
					yzUp2[nNeighUp - GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP] = CAMath::MakeFloat2(s.fDnDx * (h.Y() - y), s.fDnDx * (h.Z() - z));
				}
				else
#endif
				{
					neighUp[nNeighUp] = (calink) i;
					yzUp[nNeighUp] = CAMath::MakeFloat2(s.fDnDx * (h.Y() - y), s.fDnDx * (h.Z() - z));
				}
				if (++nNeighUp >= GPUCA_MAXN)
				{
					//printf("Neighbors buffer ran full...\n");
					break;
				}
			} while (1);

			int nNeighDn = 0;

			if (nNeighUp > 0)
			{

				int bestDn = -1, bestUp = -1;
				float bestD = 1.e10f;

				do
				{
					GPUTPCHit h;
					int i = areaDn.GetNext(tracker, rowDn, tracker.Data(), &h);
					if (i < 0) break;

					nNeighDn++;
					float2 yzdn = CAMath::MakeFloat2(s.fUpDx * (h.Y() - y), s.fUpDx * (h.Z() - z));

					for (int iUp = 0; iUp < nNeighUp; iUp++)
					{
#if defined(GPUCA_GPUCODE) & GPUCA_MAXN > GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP & GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP > 0
						float2 yzup = iUp >= GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP ? yzUp2[iUp - GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP] : yzUp[iUp];
#else
						float2 yzup = yzUp[iUp];
#endif

						float dy = yzdn.x - yzup.x;
						float dz = yzdn.y - yzup.y;
						float d = dy * dy + dz * dz;
						if (d < bestD)
						{
							bestD = d;
							bestDn = i;
							bestUp = iUp;
						}
					}
				} while (1);

				if (bestD <= chi2Cut)
				{
#if defined(GPUCA_GPUCODE) & GPUCA_MAXN > GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP & GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP > 0
					linkUp = bestUp >= GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP ? neighUp2[bestUp - GPUCA_NEIGHBOURS_FINDER_MAX_NNEIGHUP] : neighUp[bestUp];
#else
					linkUp = neighUp[bestUp];
#endif
					linkDn = bestDn;
				}
			}
		}

		tracker.SetHitLinkUpData(row, ih, linkUp);
		tracker.SetHitLinkDownData(row, ih, linkDn);
	}
}
