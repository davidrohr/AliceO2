// @(#) $Id$
// **************************************************************************
// This file is property of and copyright by the ALICE HLT Project          *
// ALICE Experiment at CERN, All rights reserved.                           *
//                                                                          *
// Primary Authors: Sergey Gorbunov <sergey.gorbunov@kip.uni-heidelberg.de> *
//                  Ivan Kisel <kisel@kip.uni-heidelberg.de>                *
//                  for The ALICE HLT Project.                              *
//                                                                          *
// Permission to use, copy, modify and distribute this software and its     *
// documentation strictly for non-commercial purposes is hereby granted     *
// without fee, provided that the above copyright notice appears in all     *
// copies and that both the copyright notice and this permission notice     *
// appear in the supporting documentation. The authors make no claims       *
// about the suitability of this software for any purpose. It is            *
// provided "as is" without express or implied warranty.                    *
//                                                                          *
//***************************************************************************

#include "AliHLTTPCCATracker.h"
#include "AliHLTTPCCAOutTrack.h"
#include "AliHLTTPCCAGrid.h"
#include "AliHLTTPCCARow.h"
#include "AliHLTTPCCATrack.h"
#include "AliHLTTPCCATracklet.h"
#include "AliHLTTPCCAMath.h"
#include "AliHLTTPCCAHit.h"
#include "MemoryAssignmentHelpers.h"

#include "TStopwatch.h"
#include "AliHLTTPCCAHitArea.h"
#include "AliHLTTPCCANeighboursFinder.h"
#include "AliHLTTPCCANeighboursCleaner.h"
#include "AliHLTTPCCAStartHitsFinder.h"
#include "AliHLTTPCCATrackletConstructor.h"
#include "AliHLTTPCCATrackletSelector.h"
#include "AliHLTTPCCAProcess.h"
#include "AliHLTTPCCASliceTrack.h"
#include "AliHLTTPCCASliceOutput.h"
#include "AliHLTTPCCADataCompressor.h"
#include "AliHLTTPCCAClusterData.h"

#include "AliHLTTPCCATrackParam.h"

#if !defined(HLTCA_GPUCODE)
#include <iostream>
#endif

//#define DRAW

#ifdef DRAW
#include "AliHLTTPCCADisplay.h"
#endif //DRAW

#ifdef HLTCA_INTERNAL_PERFORMANCE
#include "AliHLTTPCCAPerformance.h"
#endif


ClassImp( AliHLTTPCCATracker )

#if !defined(HLTCA_GPUCODE)

AliHLTTPCCATracker::AliHLTTPCCATracker()
    :
    fParam(),
    fClusterData( 0 ),
    fData(),
    fCommonMemory( 0 ),
    fCommonMemorySize( 0 ),
    fHitMemory( 0 ),
    fHitMemorySize( 0 ),
    fTrackMemory( 0 ),
    fTrackMemorySize( 0 ),
    fNTracklets( 0 ),
    fTrackletStartHits( 0 ),
    fTracklets( 0 ),
    fNTracks( 0 ),
    fTracks( 0 ),
    fNTrackHits( 0 ),
    fTrackHits( 0 ),
    fOutput( 0 ),
    fNOutTracks( 0 ),
    fOutTracks( 0 ),
    fNOutTrackHits( 0 ),
    fOutTrackHits( 0 )
{
  // constructor
}

GPUd() AliHLTTPCCATracker::~AliHLTTPCCATracker()
{
  // destructor
  delete[] fCommonMemory;
  delete[] fHitMemory;
  delete[] fTrackMemory;
}
#endif



// ----------------------------------------------------------------------------------
GPUd() void AliHLTTPCCATracker::Initialize( const AliHLTTPCCAParam &param )
{
  // initialisation
  fParam = param;
  fParam.Update();
  fData.InitializeRows( fParam );

  StartEvent();
}

GPUd() void AliHLTTPCCATracker::StartEvent()
{
  // start new event and fresh the memory

  SetupCommonMemory();
  *fNTrackHits = 0;
}

void  AliHLTTPCCATracker::SetupCommonMemory()
{
  if ( !fCommonMemory ) {
    SetPointersCommon(); // just to calculate the size
    // the 1600 extra bytes are not used unless fCommonMemorySize increases with a later event
    fCommonMemory = reinterpret_cast<char*> ( new uint4 [ fCommonMemorySize/sizeof( uint4 ) + 100] );
    SetPointersCommon();// set pointers
  }

  delete[] fHitMemory;
  fHitMemory = 0;
  delete[] fTrackMemory;
  fTrackMemory = 0;

  fData.Clear();
  *fNTracklets = 0;
  *fNTracks = 0 ;
  *fNTrackHits = 0;
  *fNOutTracks = 0;
  *fNOutTrackHits = 0;
}

GPUhd() void  AliHLTTPCCATracker::SetPointersCommon()
{
  // set all pointers to the event memory

  char *mem = fCommonMemory;
  AssignMemory( fNTracklets, mem, 1 );
  AssignMemory( fNTracks, mem, 1 );
  AssignMemory( fNTrackHits, mem, 1 );
  AssignMemory( fNOutTracks, mem, 1 );
  AssignMemory( fNOutTrackHits, mem, 1 );

  // calculate the size

  fCommonMemorySize = mem - fCommonMemory;
}


GPUhd() void  AliHLTTPCCATracker::SetPointersHits( int MaxNHits )
{
  // set all pointers to the event memory

  char *mem = fHitMemory;

  // extra arrays for tpc clusters

  AssignMemory( fTrackletStartHits, mem, MaxNHits );

  // arrays for track hits

  AssignMemory( fTrackHits, mem, 10 * MaxNHits );

  AssignMemory( fOutTrackHits, mem, 10 * MaxNHits );

  // calculate the size

  fHitMemorySize = mem - fHitMemory;
}


GPUhd() void  AliHLTTPCCATracker::SetPointersTracks( int MaxNTracks, int MaxNHits )
{
  // set all pointers to the tracks memory

  char *mem = fTrackMemory;

  // memory for tracklets

  AssignMemory( fTracklets, mem, MaxNTracks );

  // memory for selected tracks

  AssignMemory( fTracks, mem, MaxNTracks );

  // memory for output

  AlignTo < sizeof( void * ) > ( mem );
  fOutput = reinterpret_cast<AliHLTTPCCASliceOutput *>( mem );
  mem += AliHLTTPCCASliceOutput::EstimateSize( MaxNTracks, MaxNHits );

  // memory for output tracks

  AssignMemory( fOutTracks, mem, MaxNTracks );

  // calculate the size

  fTrackMemorySize = mem - fTrackMemory;
}


void AliHLTTPCCATracker::ReadEvent( AliHLTTPCCAClusterData *clusterData )
{
  fClusterData = clusterData;

  StartEvent();

  //* Convert input hits, create grids, etc.
  fData.InitFromClusterData( *clusterData );

  {
    SetPointersHits( fData.NumberOfHits() ); // to calculate the size
    fHitMemory = reinterpret_cast<char*> ( new uint4 [ fHitMemorySize/sizeof( uint4 ) + 100] );
    SetPointersHits( fData.NumberOfHits() ); // set pointers for hits
    *fNTracklets = 0;
    *fNTracks = 0 ;
    *fNOutTracks = 0;
    *fNOutTrackHits = 0;
  }
}

GPUh() void AliHLTTPCCATracker::Reconstruct()
{
  //* reconstruction of event
  //std::cout<<"Reconstruct slice "<<fParam.ISlice()<<", nHits="<<NHitsTotal()<<std::endl;

  fTimers[0] = 0; // find neighbours
  fTimers[1] = 0; // construct tracklets
  fTimers[2] = 0; // fit tracklets
  fTimers[3] = 0; // prolongation of tracklets
  fTimers[4] = 0; // selection
  fTimers[5] = 0; // write output
  fTimers[6] = 0;
  fTimers[7] = 0;

  //if( fParam.ISlice()<1 ) return; //SG!!!

  TStopwatch timer0;

  if ( NHitsTotal() < 1 ) {
    {
      SetPointersTracks( 1, 1 ); // to calculate the size
      fTrackMemory = reinterpret_cast<char*> ( new uint4 [ fTrackMemorySize/sizeof( uint4 ) + 100] );
      SetPointersTracks( 1, 1 ); // set pointers for tracks
      fOutput->SetNTracks( 0 );
      fOutput->SetNTrackClusters( 0 );
    }

    return;
  }
#ifdef DRAW

  AliHLTTPCCADisplay::Instance().ClearView();
  AliHLTTPCCADisplay::Instance().SetSliceView();
  AliHLTTPCCADisplay::Instance().SetCurrentSlice( this );
  AliHLTTPCCADisplay::Instance().DrawSlice( this, 1 );
  if ( NHitsTotal() > 0 ) {
    AliHLTTPCCADisplay::Instance().DrawSliceHits( kRed, .5 );
    AliHLTTPCCADisplay::Instance().Ask();
  }
#endif

  *fNTracks = 0;
  *fNTracklets = 0;

#if !defined(HLTCA_GPUCODE)

  AliHLTTPCCAProcess<AliHLTTPCCANeighboursFinder>( Param().NRows(), 1, *this );

#ifdef HLTCA_INTERNAL_PERFORMANCE
  //if( Param().ISlice()<=2 )
  //AliHLTTPCCAPerformance::Instance().LinkPerformance( Param().ISlice() );
#endif


#ifdef DRAW
  if ( NHitsTotal() > 0 ) {
    AliHLTTPCCADisplay::Instance().DrawSliceLinks( -1, -1, 1 );
    AliHLTTPCCADisplay::Instance().Ask();
  }
#endif


  AliHLTTPCCAProcess<AliHLTTPCCANeighboursCleaner>( Param().NRows() - 2, 1, *this );
  AliHLTTPCCAProcess<AliHLTTPCCAStartHitsFinder>( Param().NRows() - 4, 1, *this );

  int nStartHits = *fNTracklets;

  int nThreads = 128;
  int nBlocks = NHitsTotal() / nThreads + 1;
  if ( nBlocks < 12 ) {
    nBlocks = 12;
    nThreads = NHitsTotal() / 12 + 1;
    if ( nThreads % 32 ) nThreads = ( nThreads / 32 + 1 ) * 32;
  }

  nThreads = NHitsTotal();
  nBlocks = 1;

  fData.ClearHitWeights();
  //AliHLTTPCCAProcess<AliHLTTPCCAUsedHitsInitialiser>( nBlocks, nThreads, *this );


  {
    SetPointersTracks( nStartHits, NHitsTotal() ); // to calculate the size
    fTrackMemory = reinterpret_cast<char*> ( new uint4 [ fTrackMemorySize/sizeof( uint4 ) + 100] );
    SetPointersTracks( nStartHits, NHitsTotal() ); // set pointers for hits
  }

  int nMemThreads = AliHLTTPCCATrackletConstructor::NMemThreads();
  nThreads = 256;//96;
  nBlocks = nStartHits / nThreads + 1;
  if ( nBlocks < 30 ) {
    nBlocks = 30;
    nThreads = ( nStartHits ) / 30 + 1;
    if ( nThreads % 32 ) nThreads = ( nThreads / 32 + 1 ) * 32;
  }

  nThreads = nStartHits;
  nBlocks = 1;

  AliHLTTPCCAProcess1<AliHLTTPCCATrackletConstructor>( nBlocks, nMemThreads + nThreads, *this );

  //std::cout<<"Slice "<<Param().ISlice()<<": NHits="<<NHitsTotal()<<", NTracklets="<<*NTracklets()<<std::endl;

  {
    nThreads = 128;
    nBlocks = nStartHits / nThreads + 1;
    if ( nBlocks < 12 ) {
      nBlocks = 12;
      nThreads = nStartHits / 12 + 1;
      nThreads = ( nThreads / 32 + 1 ) * 32;
    }

    *fNTrackHits = 0;

    nThreads = nStartHits;
    nBlocks = 1;


    AliHLTTPCCAProcess<AliHLTTPCCATrackletSelector>( nBlocks, nThreads, *this );

    //std::cout<<"Slice "<<Param().ISlice()<<": N start hits/tracklets/tracks = "<<nStartHits<<" "<<nStartHits<<" "<<*fNTracks<<std::endl;
  }

  //std::cout<<"Memory used for slice "<<fParam.ISlice()<<" : "<<fCommonMemorySize/1024./1024.<<" + "<<fHitMemorySize/1024./1024.<<" + "<<fTrackMemorySize/1024./1024.<<" = "<<( fCommonMemorySize+fHitMemorySize+fTrackMemorySize )/1024./1024.<<" Mb "<<std::endl;


  WriteOutput();


#endif

#ifdef DRAW
  {
    AliHLTTPCCADisplay &disp = AliHLTTPCCADisplay::Instance();
    AliHLTTPCCATracker &slice = *this;
    std::cout << "N out tracks = " << *slice.NOutTracks() << std::endl;
    //disp.Ask();
    AliHLTTPCCADisplay::Instance().SetCurrentSlice( this );
    AliHLTTPCCADisplay::Instance().DrawSlice( this, 1 );
    disp.DrawSliceHits( -1, .5 );
    for ( int itr = 0; itr < *slice.NOutTracks(); itr++ ) {
      std::cout << "track N " << itr << ", nhits=" << slice.OutTracks()[itr].NHits() << std::endl;
      disp.DrawSliceOutTrack( itr, kBlue );
      //disp.Ask();
      //int id = slice.OutTracks()[itr].OrigTrackID();
      //AliHLTTPCCATrack &tr = Tracks()[id];
      //for( int ih=0; ih<tr.NHits(); ih++ ){
      //int ic = (fTrackHits[tr.FirstHitID()+ih]);
      //std::cout<<ih<<" "<<ID2IRow(ic)<<" "<<ID2IHit(ic)<<std::endl;
      //}
      //disp.DrawSliceTrack( id, kBlue );
      //disp.Ask();
    }
    disp.Ask();
  }
#endif

  timer0.Stop();
  fTimers[0] = timer0.CpuTime() / 100.;

}




GPUh() void AliHLTTPCCATracker::WriteOutput()
{
  // write output

  TStopwatch timer;

  //cout<<"output: nTracks = "<<*fNTracks<<", nHitsTotal="<<NHitsTotal()<<std::endl;

  fOutput->SetNTracks( *fNTracks );
  fOutput->SetNTrackClusters( *fNTrackHits );
  fOutput->SetPointers();

  int nStoredHits = 0;

  for ( int iTr = 0; iTr < *fNTracks; iTr++ ) {
    AliHLTTPCCATrack &iTrack = fTracks[iTr];

    AliHLTTPCCASliceTrack out;
    out.SetFirstClusterRef( nStoredHits );
    out.SetNClusters( iTrack.NHits() );
    out.SetParam( iTrack.Param() );

    fOutput->SetTrack( iTr, out );

    int iID = iTrack.FirstHitID();
    for ( int ith = 0; ith < iTrack.NHits(); ith++ ) {
      const AliHLTTPCCAHitId &ic = fTrackHits[iID + ith];
      int iRow = ic.RowIndex();
      int ih = ic.HitIndex();

      const AliHLTTPCCARow &row = fData.Row( iRow );

      //float y0 = row.Grid().YMin();
      //float z0 = row.Grid().ZMin();
      //float stepY = row.HstepY();
      //float stepZ = row.HstepZ();
      //float x = row.X();

      //const uint4 *tmpint4 = RowData() + row.FullOffset();
      //const ushort2 *hits = reinterpret_cast<const ushort2*>(tmpint4);
      //ushort2 hh = hits[ih];
      //float y = y0 + hh.x*stepY;
      //float z = z0 + hh.y*stepZ;

      int clusterIndex = fData.ClusterDataIndex( row, ih );
      int clusterRowIndex = clusterIndex - fClusterData->RowOffset( iRow );

      if ( clusterIndex < 0 || clusterIndex >= fClusterData->NumberOfClusters() ) {
        //std::cout << inpIDtot << ", " << fClusterData->NumberOfClusters()
        //<< "; " << inpID << ", " << fClusterData->NumberOfClusters( iRow ) << std::endl;
        //abort();
        continue;
      }
      if ( clusterRowIndex < 0 || clusterRowIndex >= fClusterData->NumberOfClusters( iRow ) ) {
        //std::cout << inpIDtot << ", " << fClusterData->NumberOfClusters()
        //<< "; " << inpID << ", " << fClusterData->NumberOfClusters( iRow ) << std::endl;
        //abort();
        continue;
      }

      float origX = fClusterData->X( clusterIndex );
      float origY = fClusterData->Y( clusterIndex );
      float origZ = fClusterData->Z( clusterIndex );


      unsigned int hIDrc = AliHLTTPCCADataCompressor::IRowIClu2IDrc( iRow, clusterRowIndex );

      int hltID = fClusterData->Id( clusterIndex );

      unsigned short hPackedYZ = 0;
      UChar_t hPackedAmp = 0;
      float2 hUnpackedYZ;
      hUnpackedYZ.x = origY;
      hUnpackedYZ.y = origZ;
      float hUnpackedX = origX;

      fOutput->SetClusterIDrc( nStoredHits, hIDrc  );
      fOutput->SetClusterHltID( nStoredHits, hltID  );
      fOutput->SetClusterPackedYZ( nStoredHits, hPackedYZ );
      fOutput->SetClusterPackedAmp( nStoredHits, hPackedAmp );
      fOutput->SetClusterUnpackedYZ( nStoredHits, hUnpackedYZ );
      fOutput->SetClusterUnpackedX( nStoredHits, hUnpackedX );
      nStoredHits++;
    }
  }


  // old stuff

  *fNOutTrackHits = 0;
  *fNOutTracks = 0;


  for ( int iTr = 0; iTr < *fNTracks; iTr++ ) {

    const AliHLTTPCCATrack &iTrack = fTracks[iTr];

    //std::cout<<"iTr = "<<iTr<<", nHits="<<iTrack.NHits()<<std::endl;

    //if( !iTrack.Alive() ) continue;
    if ( iTrack.NHits() < 3 ) continue;
    AliHLTTPCCAOutTrack &out = fOutTracks[*fNOutTracks];
    out.SetFirstHitRef( *fNOutTrackHits );
    out.SetNHits( 0 );
    out.SetOrigTrackID( iTr );
    out.SetStartPoint( iTrack.Param() );
    out.SetEndPoint( iTrack.Param() );

    int iID = iTrack.FirstHitID();
    int nOutTrackHitsOld = *fNOutTrackHits;

    for ( int ith = 0; ith < iTrack.NHits(); ith++ ) {
      const AliHLTTPCCAHitId &ic = fTrackHits[iID + ith];
      const AliHLTTPCCARow &row = Row( ic );
      int ih = ic.HitIndex();
      fOutTrackHits[*fNOutTrackHits] = HitInputID( row, ih );
      ( *fNOutTrackHits )++;
      //std::cout<<"write i,row,hit,id="<<ith<<", "<<ID2IRow(ic)<<", "<<ih<<", "<<HitInputID( row, ih )<<std::endl;
      if ( *fNOutTrackHits >= 10*NHitsTotal() ) {
        std::cout << "fNOutTrackHits>NHitsTotal()" << std::endl;
        //exit(0);
        return;//SG!!!
      }
      out.SetNHits( out.NHits() + 1 );
    }
    if ( out.NHits() >= 2 ) {
      ( *fNOutTracks )++;
    } else {
      ( *fNOutTrackHits ) = nOutTrackHitsOld;
    }
  }


  timer.Stop();
  fTimers[5] += timer.CpuTime();
}

GPUh() void AliHLTTPCCATracker::FitTrackFull( const AliHLTTPCCATrack &/**/, float * /**/ ) const
{
  // fit track with material
#ifdef XXX
  //* Fit the track
  FitTrack( iTrack, tt0 );
  if ( iTrack.NHits() <= 3 ) return;

  AliHLTTPCCATrackParam &t = iTrack.Param();
  AliHLTTPCCATrackParam t0 = t;

  t.Chi2() = 0;
  t.NDF() = -5;
  bool first = 1;

  int iID = iTrack.FirstHitID();
  for ( int ih = 0; ih < iTrack.NHits(); ih++, iID++ ) {
    const AliHLTTPCCAHitId &ic = fTrackHits[iID];
    int iRow = ic.rowIndex();
    const AliHLTTPCCARow &row = fData.Row( iRow );
    if ( !t0.TransportToX( row.X() ) ) continue;
    float dy, dz;
    const AliHLTTPCCAHit &h = ic.hitIndex();

    // check for wrong hits
    if ( 0 ) {
      dy = t0.GetY() - h.Y();
      dz = t0.GetZ() - h.Z();

      //if( dy*dy > 3.5*3.5*(/*t0.GetErr2Y() + */h.ErrY()*h.ErrY() ) ) continue;//SG!!!
      //if( dz*dz > 3.5*3.5*(/*t0.GetErr2Z() + */h.ErrZ()*h.ErrZ() ) ) continue;
    }

    if ( !t.TransportToX( row.X() ) ) continue;

    //* Update the track

    if ( first ) {
      t.Cov()[ 0] = .5 * .5;
      t.Cov()[ 1] = 0;
      t.Cov()[ 2] = .5 * .5;
      t.Cov()[ 3] = 0;
      t.Cov()[ 4] = 0;
      t.Cov()[ 5] = .2 * .2;
      t.Cov()[ 6] = 0;
      t.Cov()[ 7] = 0;
      t.Cov()[ 8] = 0;
      t.Cov()[ 9] = .2 * .2;
      t.Cov()[10] = 0;
      t.Cov()[11] = 0;
      t.Cov()[12] = 0;
      t.Cov()[13] = 0;
      t.Cov()[14] = .2 * .2;
      t.Chi2() = 0;
      t.NDF() = -5;
    }
    float err2Y, err2Z;
    GetErrors2( iRow, t, err2Y, err2Z );

    if ( !t.Filter2( h.Y(), h.Z(), err2Y, err2Z ) ) continue;

    first = 0;
  }
  /*
  float cosPhi = iTrack.Param().GetCosPhi();
  p0.Param().TransportToX(ID2Row( iTrack.PointID()[0] ).X());
  p2.Param().TransportToX(ID2Row( iTrack.PointID()[1] ).X());
  if( p0.Param().GetCosPhi()*cosPhi<0 ){ // change direction
  float *par = p0.Param().Par();
  float *cov = p0.Param().Cov();
  par[2] = -par[2]; // sin phi
  par[3] = -par[3]; // DzDs
    par[4] = -par[4]; // kappa
    cov[3] = -cov[3];
    cov[4] = -cov[4];
    cov[6] = -cov[6];
    cov[7] = -cov[7];
    cov[10] = -cov[10];
    cov[11] = -cov[11];
    p0.Param().CosPhi() = -p0.Param().GetCosPhi();
  }
  */
#endif
}

GPUh() void AliHLTTPCCATracker::FitTrack( const AliHLTTPCCATrack &/*track*/, float * /*t0[]*/ ) const
{
  //* Fit the track
#ifdef XXX
  AliHLTTPCCAEndPoint &p2 = ID2Point( track.PointID()[1] );
  const AliHLTTPCCAHit &c0 = ID2Hit( fTrackHits[p0.TrackHitID()].HitID() );
  const AliHLTTPCCAHit &c1 = ID2Hit( fTrackHits[track.HitID()[1]].HitID() );
  const AliHLTTPCCAHit &c2 = ID2Hit( fTrackHits[p2.TrackHitID()].HitID() );
  const AliHLTTPCCARow &row0 = ID2Row( fTrackHits[p0.TrackHitID()].HitID() );
  const AliHLTTPCCARow &row1 = ID2Row( fTrackHits[track.HitID()[1]].HitID() );
  const AliHLTTPCCARow &row2 = ID2Row( fTrackHits[p2.TrackHitID()].HitID() );
  float sp0[5] = {row0.X(), c0.Y(), c0.Z(), c0.ErrY(), c0.ErrZ() };
  float sp1[5] = {row1.X(), c1.Y(), c1.Z(), c1.ErrY(), c1.ErrZ() };
  float sp2[5] = {row2.X(), c2.Y(), c2.Z(), c2.ErrY(), c2.ErrZ() };
  //std::cout<<"Fit track, points ="<<sp0[0]<<" "<<sp0[1]<<" / "<<sp1[0]<<" "<<sp1[1]<<" / "<<sp2[0]<<" "<<sp2[1]<<std::endl;
  if ( track.NHits() >= 3 ) {
    p0.Param().ConstructXYZ3( sp0, sp1, sp2, p0.Param().CosPhi(), t0 );
    p2.Param().ConstructXYZ3( sp2, sp1, sp0, p2.Param().CosPhi(), t0 );
    //p2.Param() = p0.Param();
    //p2.Param().TransportToX(row2.X());
    //p2.Param().Par()[1] = -p2.Param().Par()[1];
    //p2.Param().Par()[4] = -p2.Param().Par()[4];
  } else {
    p0.Param().X() = row0.X();
    p0.Param().Y() = c0.Y();
    p0.Param().Z() = c0.Z();
    p0.Param().Err2Y() = c0.ErrY() * c0.ErrY();
    p0.Param().Err2Z() = c0.ErrZ() * c0.ErrZ();
    p2.Param().X() = row2.X();
    p2.Param().Y() = c2.Y();
    p2.Param().Z() = c2.Z();
    p2.Param().Err2Y() = c2.ErrY() * c2.ErrY();
    p2.Param().Err2Z() = c2.ErrZ() * c2.ErrZ();
  }
#endif
}


GPUd() void AliHLTTPCCATracker::GetErrors2( int iRow, float z, float sinPhi, float cosPhi, float DzDs, float &Err2Y, float &Err2Z ) const
{
  //
  // Use calibrated cluster error from OCDB
  //

  fParam.GetClusterErrors2( iRow, z, sinPhi, cosPhi, DzDs, Err2Y, Err2Z );
}

GPUd() void AliHLTTPCCATracker::GetErrors2( int iRow, const AliHLTTPCCATrackParam &t, float &Err2Y, float &Err2Z ) const
{
  //
  // Use calibrated cluster error from OCDB
  //

  fParam.GetClusterErrors2( iRow, t.GetZ(), t.SinPhi(), t.GetCosPhi(), t.DzDs(), Err2Y, Err2Z );
}


#if !defined(HLTCA_GPUCODE)

GPUh() void AliHLTTPCCATracker::WriteEvent( std::ostream &out )
{
  // write event to the file
  for ( int iRow = 0; iRow < fParam.NRows(); iRow++ ) {
    out << fData.Row( iRow ).HitNumberOffset() << " " << fData.Row( iRow ).NHits() << std::endl;
  }
  out << NHitsTotal() << std::endl;

  AliHLTResizableArray<float> y( NHitsTotal() ), z( NHitsTotal() );

  for ( int iRow = 0; iRow < fParam.NRows(); iRow++ ) {
    const AliHLTTPCCARow &row = Row( iRow );
    float y0 = row.Grid().YMin();
    float z0 = row.Grid().ZMin();
    float stepY = row.HstepY();
    float stepZ = row.HstepZ();
    for ( int ih = 0; ih < fData.Row( iRow ).NHits(); ih++ ) {
      int id = HitInputID( row, ih );
      y[id] = y0 + HitDataY( row, ih ) * stepY;
      z[id] = z0 + HitDataZ( row, ih ) * stepZ;
    }
  }
  for ( int ih = 0; ih < NHitsTotal(); ih++ ) {
    out << y[ih] << " " << z[ih] << std::endl;
  }
}

GPUh() void AliHLTTPCCATracker::WriteTracks( std::ostream &out )
{
  //* Write tracks to file

  out << fTimers[0] << std::endl;
  out << *fNOutTrackHits << std::endl;
  for ( int ih = 0; ih < *fNOutTrackHits; ih++ ) {
    out << fOutTrackHits[ih] << " ";
  }
  out << std::endl;

  out << *fNOutTracks << std::endl;

  for ( int itr = 0; itr < *fNOutTracks; itr++ ) {
    AliHLTTPCCAOutTrack &t = fOutTracks[itr];
    AliHLTTPCCATrackParam p1 = t.StartPoint();
    AliHLTTPCCATrackParam p2 = t.EndPoint();
    out << t.NHits() << " ";
    out << t.FirstHitRef() << " ";
    out << t.OrigTrackID() << " ";
    out << std::endl;
    out << p1.X() << " ";
    out << p1.SignCosPhi() << " ";
    out << p1.Chi2() << " ";
    out << p1.NDF() << std::endl;
    for ( int i = 0; i < 5; i++ ) out << p1.Par()[i] << " ";
    out << std::endl;
    for ( int i = 0; i < 15; i++ ) out << p1.Cov()[i] << " ";
    out << std::endl;
    out << p2.X() << " ";
    out << p2.SignCosPhi() << " ";
    out << p2.Chi2() << " ";
    out << p2.NDF() << std::endl;
    for ( int i = 0; i < 5; i++ ) out << p2.Par()[i] << " ";
    out << std::endl;
    for ( int i = 0; i < 15; i++ ) out << p2.Cov()[i] << " ";
    out << std::endl;
  }
}

GPUh() void AliHLTTPCCATracker::ReadTracks( std::istream &in )
{
  //* Read tracks  from file
  in >> fTimers[0];
  in >> *fNOutTrackHits;

  for ( int ih = 0; ih < *fNOutTrackHits; ih++ ) {
    in >> fOutTrackHits[ih];
  }
  in >> *fNOutTracks;

  for ( int itr = 0; itr < *fNOutTracks; itr++ ) {
    AliHLTTPCCAOutTrack &t = fOutTracks[itr];
    AliHLTTPCCATrackParam p1, p2;
    int i;
    float f;
    in >> i; t.SetNHits( i );
    in >> i; t.SetFirstHitRef( i );
    in >> i; t.SetOrigTrackID( i );
    in >> f; p1.SetX( f );
    in >> f; p1.SetSignCosPhi( f );
    in >> f; p1.SetChi2( f );
    in >> i; p1.SetNDF( i );
    for ( int j = 0; j < 5; j++ ) { in >> f; p1.SetPar( j, f ); }
    for ( int j = 0; j < 15; j++ ) { in >> f; p1.SetCov( j, f ); }
    in >> f; p2.SetX( f );
    in >> f; p2.SetSignCosPhi( f );
    in >> f; p2.SetChi2( f );
    in >> i; p2.SetNDF( i );
    for ( int j = 0; j < 5; j++ ) { in >> f; p2.SetPar( j, f ); }
    for ( int j = 0; j < 15; j++ ) { in >> f; p2.SetCov( j, f ); }
    t.SetStartPoint( p1 );
    t.SetEndPoint( p2 );
  }
}
#endif
