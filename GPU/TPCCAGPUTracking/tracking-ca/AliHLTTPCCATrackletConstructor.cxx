// @(#) $Id: AliHLTTPCCATrackletConstructor.cxx 27042 2008-07-02 12:06:02Z richterm $
//***************************************************************************
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
//***************************************************************************

#include "AliHLTTPCCATracker.h"
#include "AliHLTTPCCATrackParam.h"
#include "AliHLTTPCCATrackParam.h"
#include "AliHLTTPCCAGrid.h"
#include "AliHLTTPCCAHitArea.h"
#include "AliHLTTPCCAMath.h"
#include "AliHLTTPCCADef.h"
#include "AliHLTTPCCATrackletConstructor.h"


GPUd() void AliHLTTPCCATrackletConstructor::Step0 
( Int_t nBlocks, Int_t /*nThreads*/, Int_t iBlock, Int_t iThread,
  AliHLTTPCCASharedMemory &s, AliHLTTPCCAThreadMemory &r, AliHLTTPCCATracker &tracker, AliHLTTPCCATrackParam &/*tParam*/ )
{
  // reconstruction of tracklets, step 0
  
  r.fIsMemThread = ( iThread<NMemThreads() );
  if( iThread==0 ){
    Int_t nTracks = *tracker.NTracklets();
    Int_t nTrPerBlock = nTracks/nBlocks+1;
    s.fNRows = tracker.Param().NRows();
    s.fItr0 = nTrPerBlock*iBlock;
    s.fItr1 = s.fItr0 + nTrPerBlock;
    if( s.fItr1> nTracks ) s.fItr1 = nTracks;
    s.fUsedHits = tracker.HitWeights();
    s.fMinStartRow = 158;
    s.fMaxStartRow = 0;
  }
  if( iThread<32 ){
    s.fMinStartRow32[iThread] = 158;    
    s.fMaxStartRow32[iThread] = 0;
  }
}


GPUd() void AliHLTTPCCATrackletConstructor::Step1 
( Int_t /*nBlocks*/, Int_t /*nThreads*/, Int_t /*iBlock*/, Int_t iThread,
  AliHLTTPCCASharedMemory &s, AliHLTTPCCAThreadMemory &r, AliHLTTPCCATracker &tracker, AliHLTTPCCATrackParam &tParam )
{
  // reconstruction of tracklets, step 1  

  r.fItr= s.fItr0 + ( iThread - NMemThreads() ); 
  r.fGo = (!r.fIsMemThread) && ( r.fItr<s.fItr1 );
  r.fSave = r.fGo;
  r.fNHits=0;
  
  if( !r.fGo ) return;
  
  r.fStage = 0;
  Int_t header = 5+ sizeof(AliHLTTPCCATrackParam)/sizeof(int);
  r.fTrackStoreOffset = ( header + 160 )*r.fItr;  
  r.fHitStoreOffset = r.fTrackStoreOffset + header;
  
  Int_t *hitstore = ((Int_t*) tracker.Tracklets()) +r.fHitStoreOffset;
  
  UInt_t kThread = iThread %32;//& 00000020;
  if( SAVE() ) for( Int_t i=0; i<160; i++ ) hitstore[i] = -1;
    
  Int_t id = tracker.TrackletStartHits()[r.fItr];
  r.fFirstRow = AliHLTTPCCATracker::ID2IRow(id);
  r.fCurrIH =  AliHLTTPCCATracker::ID2IHit(id);
  
  CAMath::atomicMin( &s.fMinStartRow32[kThread], r.fFirstRow);    
  CAMath::atomicMax( &s.fMaxStartRow32[kThread], r.fFirstRow);    
  tParam.SinPhi() = 0;
  tParam.DzDs() = 0; 
  tParam.Kappa() = 0;
  tParam.CosPhi() = 1;
  tParam.Chi2() = 0;
  tParam.NDF() = -3;      
  
  tParam.Cov()[0] = 1; 

  tParam.Cov()[1] = 0; 
  tParam.Cov()[2] = 1; 

  tParam.Cov()[3] = 0; 
  tParam.Cov()[4] = 0; 
  tParam.Cov()[5] = 1.; 

  tParam.Cov()[6] = 0; 
  tParam.Cov()[7] = 0; 
  tParam.Cov()[8] = 0; 
  tParam.Cov()[9] = 1;
  
  tParam.Cov()[10] = 0;
  tParam.Cov()[11] = 0;
  tParam.Cov()[12] = 0;
  tParam.Cov()[13] = 0;
  tParam.Cov()[14] = 1.;

  r.fLastRow = r.fFirstRow;  
}

GPUd() void AliHLTTPCCATrackletConstructor::Step2 
( Int_t /*nBlocks*/, Int_t nThreads, Int_t /*iBlock*/, Int_t iThread,
  AliHLTTPCCASharedMemory &s, AliHLTTPCCAThreadMemory &/*r*/, AliHLTTPCCATracker &/*tracker*/, AliHLTTPCCATrackParam &/*tParam*/ )
{  
  // reconstruction of tracklets, step 2
  
  if( iThread==0 ){
    //CAMath::atomicMinGPU(&s.fMinRow, s.fMinRow32[iThread]);
    Int_t minStartRow = 158;
    Int_t maxStartRow = 0;
    Int_t n = (nThreads>32 ) ?32 :nThreads;
    for( Int_t i=0; i<n; i++ ){      
      if( s.fMinStartRow32[i]<minStartRow ) minStartRow = s.fMinStartRow32[i];
      if( s.fMaxStartRow32[i]>maxStartRow ) maxStartRow = s.fMaxStartRow32[i];
    }   
    s.fMinStartRow = minStartRow;
    s.fMaxStartRow = maxStartRow;
  } 
}

GPUd() void AliHLTTPCCATrackletConstructor::ReadData 
( Int_t iThread, AliHLTTPCCASharedMemory &s, AliHLTTPCCAThreadMemory &r, AliHLTTPCCATracker &tracker, Int_t iRow )
{    
  // reconstruction of tracklets, read data step

  if( r.fIsMemThread ){
    AliHLTTPCCARow &row = tracker.Rows()[iRow];
    Bool_t jr = !r.fCurrentData;
    Int_t n = row.FullSize();
    uint4* gMem = tracker.RowData() + row.FullOffset();
    uint4 *sMem = s.fData[jr];
    for( Int_t i=iThread; i<n; i+=NMemThreads() ) sMem[i] = gMem[i];
  } 
}


GPUd() void AliHLTTPCCATrackletConstructor::StoreTracklet 
( Int_t /*nBlocks*/, Int_t /*nThreads*/, Int_t /*iBlock*/, Int_t /*iThread*/,
  AliHLTTPCCASharedMemory &/*s*/, AliHLTTPCCAThreadMemory &r, AliHLTTPCCATracker &tracker, AliHLTTPCCATrackParam &tParam )
{    
  // reconstruction of tracklets, tracklet store step
  
  if( !r.fSave ) return;

  do{ 
    if( r.fNHits<10 ){ 
      r.fNHits = 0;
      break;
    }
    
    {  
      Bool_t ok=1;
      Float_t *c = tParam.Cov();
      for( Int_t i=0; i<15; i++ ) ok = ok && CAMath::Finite(c[i]);
      for( Int_t i=0; i<5; i++ ) ok = ok && CAMath::Finite(tParam.Par()[i]);
      ok = ok && (tParam.X()>50);
      
      if( c[0]<=0 || c[2]<=0 || c[5]<=0 || c[9]<=0 || c[14]<=0 ) ok = 0;      
      
      if(!ok){
	r.fNHits = 0;
	break; 
      }
    }   
  }while(0);
 
  if( !SAVE() ) return;
    
  Int_t *store = ((Int_t*)tracker.Tracklets()) + r.fTrackStoreOffset;
  Int_t *hitstore = ((Int_t*)tracker.Tracklets()) +r.fHitStoreOffset;
  store[0] = r.fNHits;
  
  if( r.fNHits>0 ){
    store[3] = r.fFirstRow;
    store[4] = r.fLastRow;   
    if( CAMath::Abs(tParam.Par()[4])<1.e-8 ) tParam.Par()[4] = 1.e-8;
    *((AliHLTTPCCATrackParam*)(store+5)) = tParam;
    Int_t w = (r.fNHits<<16)+r.fItr;
    for( Int_t iRow=0; iRow<160; iRow++ ){
      Int_t ih = hitstore[iRow];
      if( ih>=0 ){
	Int_t ihTot = tracker.Rows()[iRow].FirstHit() + ih;
	CAMath::atomicMax( tracker.HitWeights() + ihTot, w );
      }
    }
  }  
}

GPUd() void AliHLTTPCCATrackletConstructor::UpdateTracklet
( Int_t /*nBlocks*/, Int_t /*nThreads*/, Int_t /*iBlock*/, Int_t /*iThread*/,
    AliHLTTPCCASharedMemory &s, AliHLTTPCCAThreadMemory &r, AliHLTTPCCATracker &tracker, AliHLTTPCCATrackParam &tParam,Int_t iRow )
{  
  // reconstruction of tracklets, tracklets update step

  if( !r.fGo ) return;
	  
  const Int_t kMaxRowGap = 5;	
  
  Int_t *hitstore = ((Int_t*) tracker.Tracklets()) +r.fHitStoreOffset;
  
  AliHLTTPCCARow &row = tracker.Rows()[iRow];
  
  Float_t y0 = row.Grid().YMin();
  Float_t stepY = row.HstepY();
  Float_t z0 = row.Grid().ZMin();
  Float_t stepZ = row.HstepZ();
  Float_t stepYi = row.HstepYi();
  Float_t stepZi = row.HstepZi();	  
  
  if( r.fStage == 0 ){ // fitting part	    
    do{
      
      if( iRow<r.fFirstRow || r.fCurrIH<0  ) break;

      uint4 *tmpint4 = s.fData[r.fCurrentData];   
      ushort2 hh = reinterpret_cast<ushort2*>(tmpint4)[r.fCurrIH];
      
      Int_t oldIH = r.fCurrIH;
      r.fCurrIH = reinterpret_cast<Short_t*>(tmpint4)[row.FullLinkOffset() + r.fCurrIH];	  
      
      Float_t x = row.X();
      Float_t y = y0 + hh.x*stepY;
      Float_t z = z0 + hh.y*stepZ;
     
      if( iRow==r.fFirstRow ){
	tParam.X() = x;
	tParam.Y() = y;
	tParam.Z() = z;
	Float_t err2Y, err2Z;	  
 	tracker.GetErrors2( iRow, tParam, err2Y, err2Z );
	tParam.Cov()[0] = err2Y;
	tParam.Cov()[2] = err2Z;
      }else{	    
	if( !tParam.TransportToX0( x, .95 ) ){ 
	  if( SAVE() ) hitstore[iRow] = -1; 
	  break; 
	}
	Float_t err2Y, err2Z;
	tracker.GetErrors2( iRow, *((AliHLTTPCCATrackParam*)&tParam), err2Y, err2Z );
	if( !tParam.Filter20( y, z, err2Y, err2Z, .95 ) ) { 
	  if( SAVE() ) hitstore[iRow] = -1; 
	  break; 
	}	   
      }
      if( SAVE() ) hitstore[iRow] = oldIH; 
      r.fNHits++;
      r.fLastRow = iRow;
      if( r.fCurrIH<0 ){
	r.fStage = 1;	 
	if( r.fNHits<3 ){ r.fNHits=0; r.fGo = 0;}
      } 
      break;
    } while(0);
  }
  else // forward/backward searching part
    {	        
      do{ 
	if( r.fStage == 2 && iRow>=r.fFirstRow ) break; 
	if( r.fNMissed>kMaxRowGap  ){ 
	  r.fGo = 0; 
	  break;
	}
	
	r.fNMissed++;	
	
	Float_t x = row.X();
	Float_t err2Y, err2Z;
	if( !tParam.TransportToX0( x, .95 ) ) break;
	uint4 *tmpint4 = s.fData[r.fCurrentData];   

	ushort2 *hits = reinterpret_cast<ushort2*>(tmpint4);
		
	Float_t fY = tParam.GetY();
	Float_t fZ = tParam.GetZ();
	Int_t best = -1;		
	
	{ // search for the closest hit
	  
	  Int_t ds;
	  Int_t fY0 = (Int_t) ((fY - y0)*stepYi);
	  Int_t fZ0 = (Int_t) ((fZ - z0)*stepZi);
	  Int_t ds0 = ( ((int)1)<<30);
	  ds = ds0;
	  
	  UInt_t fIndYmin;
	  UInt_t fHitYfst=1, fHitYlst=0, fHitYfst1=1, fHitYlst1=0;
	  
	  fIndYmin = row.Grid().GetBin( (float)(fY-1.), (float)(fZ-1.) );

	  {	  	    
	    Int_t nY = row.Grid().Ny();
	    UShort_t *sGridP = (reinterpret_cast<UShort_t*>(tmpint4)) + row.FullGridOffset();
	    fHitYfst = sGridP[fIndYmin];
	    fHitYlst = sGridP[fIndYmin+2];
	    fHitYfst1 = sGridP[fIndYmin+nY];
	    fHitYlst1 = sGridP[fIndYmin+nY+2];
	  }
	  
	  for( UInt_t fIh = fHitYfst; fIh<fHitYlst; fIh++ ){
	    ushort2 hh = hits[fIh];	  
	    Int_t ddy = (Int_t)(hh.x) - fY0;
	    Int_t ddz = (Int_t)(hh.y) - fZ0;
	    Int_t dds = CAMath::mul24(ddy,ddy) + CAMath::mul24(ddz,ddz);
	    if( dds<ds ){
	      ds = dds;
	      best = fIh;
	    }	    	      
	  }
	  	  
	  for( UInt_t fIh = fHitYfst1; fIh<fHitYlst1; fIh++ ){
	    ushort2 hh = hits[fIh];
	    Int_t ddy = (Int_t)(hh.x) - fY0;
	    Int_t ddz = (Int_t)(hh.y) - fZ0;
	    Int_t dds = CAMath::mul24(ddy,ddy) + CAMath::mul24(ddz,ddz);
	    if( dds<ds ){
	      ds = dds;
	      best = fIh;
	    }	    	      	      
	  }
	}// end of search for the closest hit
	
	if( best<0 ) break;	
	
	ushort2 hh = hits[best];
	
	tracker.GetErrors2( iRow, *((AliHLTTPCCATrackParam*)&tParam), err2Y, err2Z );
	
	Float_t y = y0 + hh.x*stepY;
	Float_t z = z0 + hh.y*stepZ;
	Float_t dy = y - fY;
	Float_t dz = z - fZ;	  
	
	const Float_t kFactor = 3.5*3.5;
	Float_t sy2 = kFactor*( tParam.GetErr2Y() +  err2Y );
	Float_t sz2 = kFactor*( tParam.GetErr2Z() +  err2Z );
	if( sy2 > 1. ) sy2 = 1.;
	if( sz2 > 1. ) sz2 = 1.;
	if( iRow==63 || iRow==64 || iRow==65 ){
	  if( sy2 < 4. ) sy2 = 4.;
	  if( sz2 < 4. ) sz2 = 4.;
	}
	
	
	if( CAMath::fmul_rz(dy,dy)>sy2 || CAMath::fmul_rz(dz,dz)>sz2  ) break;
	
	if( !tParam.Filter20( y, z, err2Y, err2Z, .95 ) ) break;

	if( SAVE() ) hitstore[ iRow ] = best;
	r.fNHits++;          
	r.fNMissed=0;		
      }while(0);
    }
}



GPUd() void AliHLTTPCCATrackletConstructor::Thread
( Int_t nBlocks, Int_t nThreads, Int_t iBlock, Int_t iThread, Int_t iSync,
  AliHLTTPCCASharedMemory &s, AliHLTTPCCAThreadMemory &r, AliHLTTPCCATracker &tracker, AliHLTTPCCATrackParam &tParam )
{    

  // reconstruction of tracklets
  if( iSync==0 )
    {  
      Step0( nBlocks, nThreads, iBlock, iThread, s, r, tracker, tParam );
    }
  else if( iSync==1 )
    { 
      Step1( nBlocks, nThreads, iBlock, iThread, s, r, tracker, tParam );
    }
  else if( iSync==2 )
    {
      Step2( nBlocks, nThreads, iBlock, iThread, s, r, tracker, tParam );
    }
  
  else if( iSync==3 )
    
    {
      r.fCurrentData = 1;
      ReadData( iThread, s, r, tracker, s.fMinStartRow );
      r.fCurrentData = 0;
      r.fNMissed = 0;           
    }
  else if( iSync==3+159 +1 )    
    {
      r.fCurrentData = 1;
      Int_t nextRow = s.fMaxStartRow-1;
      if( nextRow<0 ) nextRow = 0;
      ReadData( iThread, s, r, tracker, nextRow );
      r.fCurrentData = 0;
      r.fNMissed = 0;           
      r.fStage = 2;
    }
  
  else if( iSync<=3+159+1+159 )    
    {      
      Int_t iRow, nextRow;
      if(  iSync<=3+159 ){
	iRow = iSync - 4;
	if( iRow < s.fMinStartRow ) return;
	nextRow = iRow+1;
	if( nextRow>158 ) nextRow = 158;
      }else{
 	iRow = 158 - (iSync - 4-159-1);
	if( iRow >= s.fMaxStartRow ) return;
	nextRow = iRow-1;
 	if( nextRow<0 ) nextRow = 0;
      }
      
      if( r.fIsMemThread ){
	ReadData( iThread, s, r, tracker, nextRow );  
      }else{
	UpdateTracklet( nBlocks, nThreads, iBlock, iThread,
			s, r, tracker, tParam, iRow );
      }
      r.fCurrentData = !r.fCurrentData;          
    }    
  
  else if( iSync== 4+159*2 +1+1 ) //     
    {
      StoreTracklet( nBlocks, nThreads, iBlock, iThread, 
		     s, r, tracker, tParam );
    }
}

