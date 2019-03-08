// diff3DUtil.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diff3DUtil.h"

typedef struct
{
   V3I   def;
   Stride stride[3], step[26];
   MMV3I mm;
   size_t n;
} MapOrg;

const D3MapElem gExtMask= 0x3f << 26;


/***/

size_t dotS3 (Index x, Index y, Index z, const Stride s[3]) { return( (size_t) (x*s[0]) + y*s[1] + z*s[2] ); }
size_t d2I3 (int dx, int dy, int dz) { return( dx*dx + dy*dy + dz*dz ); }

static size_t thresholdNU8 (U8 *pR, const U8 *pS, size_t n, const U8 t, const U8 v[2])
{
   size_t s=0;
   for (size_t i= 0; i<n; i++) { s+= pR[i]= v[ (pS[i] > t) ]; }
   return(s);
} // thresholdNU8

static void transLXNU8 (U8 *pR, const U8 *pS, size_t n, const U16 lm[2])
{
   for (size_t i= 0; i<n; i++) { pR[i]= ((pS[i] * lm[0]) + lm[1]) >> 8; }
} // transSANU8

static int findIdxT (const size_t h[], int maxH, size_t t)
{
   size_t s=0;
   int i= 0;//printf("%d:%zu\n", i, s); 
   do { s+= h[i]; i+= (s < t); } while ((s < t) && (i < maxH));
   return(i);
} // findIdxT

static int popT (int t[], const U8 *pS, const size_t n, const float popF[], const int nF)
{
   size_t h[256]={0};
   //int iR= 0;
   for (size_t i= 0; i<n; i++) { h[ pS[i] ]++; }
   for (int iF= 0; iF<nF; iF++)
   {
      float f= popF[iF];
      if ((f > 0) && (f <= 1)) { f*= n; }
      t[iF]= findIdxT(h, 256, f);
      printf(" popT%d/%d %G -> %d\n", iF, nF, f, t[iF]);
   }
   return(nF);
} // popT

static size_t imgU8PopTransferPerm (U8 *pPerm, const U8 *pS, const size_t n, const float popF[2], const int maxPerm)
{
   size_t s= 0;
   int iT[2]={0,0};
   int nT= popT(iT, pS, n, popF, 1+(popF[1] < popF[0]) );
   if ((iT[0] > 0) && (iT[0] < 256))
   {
      float lm[2];
      lm[0]= (float)(maxPerm - 0) / (iT[1] - iT[0]);
      lm[1]= maxPerm - lm[0] * iT[1];
      printf("perm %Gx+%G\n", lm[0],lm[1]);
      int v[2]= { iT[0]*lm[0]+lm[1], iT[1]*lm[0]+lm[1] };
      printf(" %d->%d %d->%d\n", iT[0],v[0],iT[1],v[1]);
      for (size_t i= 0; i<n; i++)
      {
         int v= lm[0] * pS[i] + lm[1]; 
         if (v >= 1) { pPerm[i]= MIN(v,maxPerm); s++; } else { pPerm[i]= 0; }
      }
   }
   return(s);
} // imgU8PopTransferPerm

/*
static size_t map6TransferU8 (D3S6MapElem * restrict pM, const U8 *pU8, const size_t n, const U8 t)
{
static const D3S6MapElem v6[2]= { (1<<6)-1, 0 };
   return( thresholdNU8(pM, pU8, n, t, v6) / v6[0] );
} // map6TransferU8
*/

static void permTransferMapNH (void * pPNH, size_t s[256], const U8 nMB, const U8 nNH, const I8 nP, const U8 *pPerm, const size_t n)
{
   const D3MapElem mNH= (1<<nNH)-1;
   const D3MapElem mP= (1<<nP)-1;
   
   switch (nMB)
   {
      case 4 :
      {
         D3MapElem *pM= pPNH;
         for (size_t i=0; i<n; i++)
         { 
            D3MapElem v= pPerm[i] & mP;
            s[v]++;
            pM[i]= mNH | (v << nNH); 
         }
         break;
      }
      case 1 :
      {
         U8 *pM= pPNH;
         for (size_t i=0; i<n; i++)
         { 
            U8 v= pPerm[i] & mP;
            s[v]++;
            pM[i]= mNH | (v << nNH); 
         }
         break;
      }
   }
} // permTransferMapNH

// INLINE ?
static MBVal andBytesLE (U8 *pB, const size_t idx, const U8 nB, const MBVal v)
{
   pB+= idx * nB; // NB index refers to multi-byte quantity
   return writeBytesLE(pB, 0, nB, v & readBytesLE(pB, 0, nB) );
} // andBytesLE

static void sealBoundaryMapNH (void *pM, const U8 nBV, const MapOrg *pO, const D3MapElem extMask)
{
   for (Index y= pO->mm.vMin.y; y <= pO->mm.vMax.y; y++)
   {
      for (Index x= pO->mm.vMin.x; x <= pO->mm.vMax.x; x++)
      {
         const size_t i= dotS3(x, y, pO->mm.vMin.z, pO->stride);
         const size_t j= dotS3(x, y, pO->mm.vMax.z, pO->stride);
         andBytesLE(pM, i, nBV, extMask|getBoundaryM26(x, y, pO->mm.vMin.z, &(pO->mm)));
         andBytesLE(pM, j, nBV, extMask|getBoundaryM26(x, y, pO->mm.vMax.z, &(pO->mm)));
      }
   }
   for (Index z= pO->mm.vMin.z; z <= pO->mm.vMax.z; z++)
   {
      for (Index x= pO->mm.vMin.x; x <= pO->mm.vMax.x; x++)
      {
         const size_t i= dotS3(x, pO->mm.vMin.y, z, pO->stride);
         const size_t j= dotS3(x, pO->mm.vMax.y, z, pO->stride);
         andBytesLE(pM, i, nBV, extMask|getBoundaryM26(x, pO->mm.vMin.y, z, &(pO->mm)));
         andBytesLE(pM, j, nBV, extMask|getBoundaryM26(x, pO->mm.vMax.y, z, &(pO->mm)));
      }
      for (Index y= pO->mm.vMin.y; y <= pO->mm.vMax.y; y++)
      {
         const size_t i= dotS3(pO->mm.vMin.x, y, z, pO->stride);
         const size_t j= dotS3(pO->mm.vMax.x, y, z, pO->stride);
         //pM[i]&= extMask|getBoundaryM26(pO->mm.vMin.x, y, z, &(pO->mm));
         //pM[j]&= extMask|getBoundaryM26(pO->mm.vMax.x, y, z, &(pO->mm));
         andBytesLE(pM, i, nBV, extMask|getBoundaryM26(pO->mm.vMin.x, y, z, &(pO->mm)));
         andBytesLE(pM, j, nBV, extMask|getBoundaryM26(pO->mm.vMax.x, y, z, &(pO->mm)));
      }
   }
} // sealBoundaryMapNH


static size_t constrainNH (D3MapElem * const pM, const U8 nNH, const U8 * const pU8, const size_t n, 
                     const Stride step[], const D3MapElem revM[])
{
   const D3MapElem nhM= (1 << nNH) - 1;
   size_t w=0;
   for (size_t i= 0; i<n; i++)
   {
      if (0 == pU8[i])
      {
         const D3MapElem m= pM[i];
         w+= (0 == (m & nhM));
         for (U8 j=0; j < nNH; j++)
         {  // adjust all available neighbours, preventing flux into site
            if (m & (1<<j)) { pM[ i + step[j] ] &= revM[j]; }
         }
         pM[i]= 0;
      }
   }
   return(w);
} // constrainNH

static size_t constrainNHB (void * const pM, const U8 nMB, const U8 nNH, const U8 * const pU8, const size_t n, 
                     const Stride step[], const D3MapElem revM[])
{
   const D3MapElem nhM= (1 << nNH) - 1;
   size_t w=0;
   for (size_t i= 0; i<n; i++)
   {
      if (0 == pU8[i])
      {
         const size_t iB= i * nMB;
         const D3MapElem m= readBytesLE(pM, iB, nMB);
         w+= (0 == (m & nhM)); if (0 == (m & nhM)) { printf("\n***%zu?\n\n", i); }
         for (U8 j=0; j < nNH; j++)
         {  // adjust all available neighbours, preventing flux into site
            if (m & (1<<j)) { andBytesLE(pM, i + step[j], nMB, revM[j]); }
         }
         writeBytesLE(pM, iB, nMB, 0);
      }
   }
   return(w);
} // constrainNHB

static void genRevM (D3MapElem revM[], char n)
{
   const char revD[2]={+1,-1}; // even +1, odd -1 : 0->1, 1->0, 2->3, 3->2 ...
   // build reverse mask table
   for (int i=0; i < n; i++)
   {
      revM[i]= ~( 1 << ( i + revD[ (i&1) ] ) );
   }
} // genRevM

static void constrainMapNH (void * const pM, const U8 nMB, const U8 nNH, const U8 * pU8, const MapOrg *pO)
{
   D3MapElem revM[26];

   if (nNH > (nMB<<3)) { printf("ERROR: constrainMapNH() - nNH=%u, nMB=%u\n", nNH, nMB); }
   if (nNH <= 26)
   {
      size_t w=0;
      genRevM(revM,nNH);
      if (4 == nMB) { w= constrainNH(pM, nNH, pU8, pO->n, pO->step, revM); }
      else { w= constrainNHB(pM, nMB, nNH, pU8, pO->n, pO->step, revM); }
      if (w > 0) { printf("WARNING: constrainMapNH() - %zu degenerate map entries\n", w);}
   }
} // constrainMapNH

#include "diffTestUtil.h"

float processMap (void * pM, const U8 nMapBytes, const U8 nNHBits, const U8 * pPerm, const MapOrg * pO, U8 v)
{
   size_t pd[256]={0,};

   printf("transfer...\n");
   permTransferMapNH(pM, pd, nMapBytes, nNHBits, 8*nMapBytes-nNHBits, pPerm, pO->n);
   if (v > 0)
   {
      float r= 100.0 / pO->n;
      printf("PermDist:\n");
      for (int i=0; i<256; i++)
      {
         if (pd[i] > 0) { printf("%d: %12zu = %G%%\n", i, pd[i], r * pd[i]); } 
      }
      printf("\n");
      if (4 == nMapBytes) { dumpDMMBC(pPerm, pM, pO->n, -1); }
   }
   printf("seal...\n");
   sealBoundaryMapNH(pM, nMapBytes, pO, gExtMask);
   if ((v > 0) && (4 == nMapBytes)) { dumpDMMBC(pPerm, pM, pO->n, (1<<nNHBits)-1); }

   printf("constrain...\n");
   constrainMapNH(pM, nMapBytes, nNHBits, pPerm, pO);
   if ((v > 0) && (4 == nMapBytes)) { dumpDMMBC(pPerm, pM, pO->n, (1<<nNHBits)-1); }

   return((float)(pO->n - pd[0]) / pO->n );
} // processMap

static void step6FromStride (Stride step[6], const Stride stride[3])
{
   step[0]= -stride[0];
   step[1]=  stride[0];
   step[2]= -stride[1];
   step[3]=  stride[1];
   step[4]= -stride[2];
   step[5]=  stride[2];
} // step6FromStride

static size_t initMapOrg (MapOrg *pO, const V3I *pD)
{
   pO->def= *pD;

   pO->stride[0]= 1;
   pO->stride[1]= pO->def.x;
   pO->stride[2]= pO->def.x * pO->def.y;

   step6FromStride(pO->step, pO->stride);
   diffSet6To26(pO->step);
   //printf("initMapOrg() - s26m[]=\n");
   //for (int i=0; i<26; i++) { printf("%d\n", pO->step[i]); }

   pO->mm.vMin.x= pO->mm.vMin.y= pO->mm.vMin.z= 0;
   pO->mm.vMax.x= pO->def.x-1;
   pO->mm.vMax.y= pO->def.y-1;
   pO->mm.vMax.z= pO->def.z-1;

   pO->n= (size_t)pO->stride[2] * pO->def.z;
   return(pO->n);
} // initMapOrg

void adjustMMV3I (MMV3I *pR, const MMV3I *pS, const I32 a)
{
   pR->vMin.x= pS->vMin.x - a;
   pR->vMin.y= pS->vMin.y - a;
   pR->vMin.z= pS->vMin.z - a;
   pR->vMax.x= pS->vMax.x + a;
   pR->vMax.y= pS->vMax.y + a;
   pR->vMax.z= pS->vMax.z + a;
} // adjustMMV3I

size_t initDiffOrg (DiffOrg *pO, U32 def, U32 nP)
{
   size_t n= 1;

   //def= MAX(32,def);
   //nP= MAX(1,nP);
   pO->def.x= pO->def.y= pO->def.z= def; 
   pO->nPhase= nP;
   for (int i=0; i<DIFF_DIM; i++)
   {
      pO->stride[i]= n; // planar
      n*= def; 
   }
   pO->phaseStride= n; // planar

   // if (interleaved) { pC->org.phaseStride= 1; for (int i=0; i<DIFF_DIM; i++) { pC->org.stride[i]*= nP } }
   step6FromStride(pO->step, pO->stride);

   pO->n1P= pO->def.x * pO->def.y; // HACKY! check interleaving!
   pO->n1F= n;
   n*= pO->nPhase;
   pO->n1B= n;
   return(n);
} // initDiffOrg

DiffScalar initIsoW (DiffScalar w[], DiffScalar r, U32 nHood, U32 f)
{
   DiffScalar t= 0;
   U32 n[3]= {6,0,0};

   w[0]= w[1]= w[2]= w[3]= 0;
   switch (nHood)
   {
      case 6 : w[1]= r / 6; break;
      case 14 :   // 6 + 8/3 = 26/3
         n[1]= 8;
         w[1]= (r * 3) / 26; // 6 * 3/26 = 18/26
         w[2]= r / 26;       // 8 * 1/26 = 8/26
         break;                        // 26/26
      case 18 :   // 6 + 12/2 = 24/2
         n[1]= 12;
         w[1]= (r * 2) / 24;  // 6 * 2/24  = 12/24
         w[2]= r / 24;        // 12 * 1/24 = 12/24
         break;                         // 24/24
      case 26 :   // 6 + 12/2 + 8/3 = (36+36+16)/6 = 88/6
         n[1]= 12;
         n[2]= 8;
         w[1]= (r * 6) / 88;  // 6 * 6/88  = 36/88
         w[2]= (r * 3) / 88;  // 12 * 3/88 = 36/88
         w[3]= (r * 2) / 88;  // 8 * 2/88  = 16/88
         break;                         // 88/88
      default : return(0);
   }

   t= (n[0] * w[1] + n[1] * w[2] + n[2] * w[3]);
   w[0]= 1 - t;
   if (f & 1) { printf("initIsoW() - w[]= %G, %G, %G, %G\n", w[0], w[1], w[2], w[3]); }
   return(t);
} // initIsoW


float setDefaultMap (D3MapElem *pM, MapDesc *pMD, const V3I *pD, const U32 id)
{
   MapOrg org;
   size_t n= initMapOrg(&org, pD);
   const D3MapElem me= getBoundaryM26V(org.def.x/2, org.def.y/2, org.def.z/2, &(org.mm));

   for (size_t i=0; i < n; i++) { pM[i]= me; }
   sealBoundaryMapNH(pM, sizeof(*pM), &org, 0);

   switch (id)
   {
      case 1 :
      {
#if 1
         D3MapElem revM[26];
         MMV3I mm;
         size_t j;
         Index x;
         genRevM(revM,26);
         adjustMMV3I(&mm, &(org.mm), -1); //yz);
         mm.vMax.y-= 0.495 * org.def.y;

         x= 0.495 * org.def.x;
         for (Index z= mm.vMin.z; z < mm.vMax.z; z++)
         {
            for (Index y= mm.vMin.y; y < mm.vMax.y; y++)
            {
               const size_t j= dotS3(x,y,z, org.stride);
               // update all neighbours of obstruction
               for (int i=0; i < 26; i++) { pM[j + org.step[i]]&= revM[i]; }
               pM[j]= 0;  // mark obstruction as impermeable and inert
            }
         }
#else
         adjustMapN(pM, &org, 26, gExtMask);
         //printf("adjustMap26() - %zu\n", adjustMap26(pM, &org) );
#endif
         break;
      }
   }
   pMD->mapBytes=4; pMD->nHood=26; pMD->maxPermSet=0; pMD->permAlign= 26;
#if 1
   size_t t= 0;
   for (size_t i=0; i < n; i++) { t+= bitCountZ(pM[i] ^ me); }
   printf("setDefaultMap() - %zu\n",t);
#endif
   return(1);
} // setDefaultMap

Bool32 getProfileRM (RawTransMethodDesc *pRM, const U8 idT, const U8 idM, const U8 f)
{
   if (idT <= 2)
   {
      U8 mBytes=0, nHood=0, bitsFree=0;
      switch(idM)
      {
         case MAP_ID_B1NH6 : mBytes= 1; nHood= 6;  bitsFree= 2;  break;
         case MAP_ID_B4NH6 : mBytes= 4; nHood= 6;  bitsFree= 26; break;
         case MAP_ID_B4NH18 : mBytes= 4; nHood= 18; bitsFree= 14; break;
         case MAP_ID_B4NH26 : mBytes= 4; nHood= 26; bitsFree= 6;  break;   
         default : return(FALSE);
      }
      switch(idT)
      {
         case TFR_ID_RAW : pRM->method= 0; pRM->maxPermLvl= 1;
            pRM->param[0]= 0.0; pRM->param[0]= 0.0; 
            break;
         case TFR_ID_THRESHOLD : pRM->method= 1; pRM->maxPermLvl= 1; 
            pRM->param[0]= 100.0; pRM->param[0]= 0.0; 
            break;
         case TFR_ID_PDFPERM :
         {
            U8 mbf= MIN(bitsFree,3);
            bitsFree-= mbf;
            pRM->method= 2; pRM->maxPermLvl= (1<<mbf)-1;
            pRM->param[0]= 0.35; pRM->param[1]= 0.20; 
            break;
         }
         default : return(FALSE);
      }
      pRM->flags= f;
      pRM->nBNH= nHood | (mBytes<<5);
      return(TRUE);
   }
   return(FALSE);
} // getProfileRM

static int findNear (V3I * pV, const V3I *pC, const U8 * pPerm, const MapOrg * pO)
{
   MMV3I mm;
   V3I i;
   size_t r=-1;
   U8 t= 1;

   adjustMMV3I(&mm, &(pO->mm), -1);
   for (i.z=mm.vMin.z; i.z < mm.vMax.z; i.z++)
   {
      for (i.y=mm.vMin.y; i.y < mm.vMax.y; i.y++)
      {
         for (i.x=mm.vMin.x; i.x < mm.vMax.x; i.x++)
         {
            const size_t j= dotS3(i.x, i.y, i.z, pO->stride);
            if (pPerm[j] >= t)
            {
               size_t d= d2I3( i.x - pC->x, i.y - pC->y, i.z - pC->z );
               if (d < r) { r= d; *pV= i; t= pPerm[j]; }
            }
         }
      }
   }
   if (r < -1) { return(t); }
   //else 
   return(0);
} // findNear

U32 findIMD (const U32 u[], const U32 n, const U32 v)
{
   U32 iM=0, m= u[0] - v;
   for (U32 i=1; i<n; i++)
   {
      U32 d= u[i] - v;
      if (d < m) { m= d; iM= i; }
   }
   return(iM);
} // findIMD

static int findInnoc (V3I * pV, const U8 * pPerm, const MapOrg * pO)
{
   MMV3I mm;
   V3I i;
   StatMomD3R2 sm3={0,};
   StatResD1R2 r[3];

   adjustMMV3I(&mm, &(pO->mm), -1);
   for (i.z=mm.vMin.z; i.z < mm.vMax.z; i.z++)
   {
      for (i.y=mm.vMin.y; i.y < mm.vMax.y; i.y++)
      {
         for (i.x=mm.vMin.x; i.x < mm.vMax.x; i.x++)
         {
            const size_t j= dotS3(i.x,i.y,i.z,pO->stride);
            statMom3AddW(&sm3, i.x, i.y, i.z, pPerm[j]);
         }
      }
   }
   
   statMom3Res1(r, &sm3, 0);
   i.x= r[0].m;
   i.y= r[1].m;
   i.z= r[2].m;
   *pV= i; 
   size_t j= dotS3(i.x,i.y,i.z,pO->stride);
   //if (pPerm[j] > 0) { }
   return(pPerm[j]);
} // findInnoc

#include "cluster.h"

float mapFromU8Raw (void *pM, MapDesc *pMD, const MemBuff *pWS, const char *path, 
      const RawTransMethodDesc *pRM, const DiffOrg *pO)
{
   float r=0;
   size_t bytes= fileSize(path);
   char m;
   MapOrg org;
   U8 verbose=1;

   memset(pMD, 0, sizeof(*pMD));
   if (initMapOrg(&org, &(pO->def)) > 0)
   {
      const V3I c= {org.def.x / 2, org.def.y / 2, org.def.z / 2};
      U8 *pRaw= pWS->p;
      bytes= loadBuff(pRaw, path, MIN(bytes, pWS->bytes));
      printf("mapFromU8Raw() - %G%cBytes\n", binSizeZ(&m, bytes), m);
      if (bytes >= org.n)
      {
         U8 *pPerm= pRaw;
         pMD->nHood= pRM->nBNH & 0x1F;
         pMD->mapBytes= pRM->nBNH >> 5;
         switch(pRM->method)
         {
            case TFR_ID_THRESHOLD :
            {
               const U8 v2[2]={1,0};
               if (thresholdNU8(pPerm, pRaw, org.n, pRM->param[0], v2) > 0) { pMD->maxPermSet= 1; }
               break;
            }
            case TFR_ID_PDFPERM :
               imgU8PopTransferPerm(pPerm, pRaw, org.n, pRM->param, pRM->maxPermLvl);
               pMD->maxPermSet= pRM->maxPermLvl; // HACK!
               break;
            default : pMD->maxPermSet= 0;
               break; // TFR_ID_RAW
         }
         r= processMap(pM, pMD->mapBytes, pMD->nHood, pPerm, &org, verbose);
         if (0 == findInnoc(&(pMD->site), pPerm, &org) )
         {
            findNear(&(pMD->site), &c, pPerm, &org);
         }
         if (pRM->flags & D3UF_PERM_SAVE)
         {
            const char *name= "perm256u8.raw";
            U16 lm[2];
            if ((pRM->maxPermLvl > 0) && (pRM->maxPermLvl < 0xFF))
            {  // Maximise visual discrimination of levels by (fixed point) scaling
               lm[0]= 0xFF00 / pRM->maxPermLvl; lm[1]= 0; // fixed point linear map
               transLXNU8(pRaw, pPerm, org.n, lm);
            }
            bytes= saveBuff(pRaw, name, org.n);
            printf("%s %G%cBytes\n", name, binSizeZ(&m,bytes), m);
         }
         if (pRM->flags & D3UF_CLUSTER_TEST)
         {
            U8 v[2]={0,1};
            thresholdNU8(pPerm, pPerm, org.n, 0, v); // (perm>0) -> 1
            MemBuff ws= *pWS;

            ws.w+=      org.n; // * sizeof(*pPerm);
            ws.bytes-=  org.n;
            {
               U32 *pI, *pC, nC, mN, mC, iM=-1;
               //testC(org.stride);
               U32 m= ws.bytes / sizeof(U32);
               mN= org.n;
               mC= 0.5 * org.n;
               while ((mN+mC) > m)
               { 
                  float f= (m / (mN+mC));
                  mN*= f;
                  mC*= f;
               }

               pC= ws.p;
               pC[0]= 0; pC++;
               pI= pC+mC+1; mN-= 1;
               printf("Cluster prep max: %unodes %uclusters\n", mN, mC);
               nC= clusterXDBFN6(pI, pC, pPerm, &(org.def), org.stride);
               printf("Found: %unodes %uclusters\n", pC[nC-1], nC);
               m= 500;
               mC= 0;
               for (U32 i=1; i < nC; i++)
               {
                  U32 s= pC[i] - pC[i-1];
                  if (s >= m)
                  {
                     printf("C%u : %u\n", i, s);
                     if (s > mC) { iM= i; mC= s; }
                  }
               }
               if (iM < nC)
               {
                  U32 v0= pC[iM-1];
                  U32 v1= pC[iM];
                  U32 s= v1 - v0;
                  U32 ve= pC[nC];
                  printf("Max: C%u : %u (%G%%)\n", iM, s, s * 100.0 / org.n);
                  
                  v0= pC[iM-1];
               
                  mN= findIMD(pI+v0, s, dotS3(c.x,c.y,c.z,org.stride));
//pC[iM-1] + s >> 1); // median
                  split3(&(pMD->site.x), pI[mN], org.stride);
                  printf("Mid: [%u] : %u -> (%d,%d,%d)\n", mN, pI[mN], pMD->site.x, pMD->site.y, pMD->site.z);

                  for (U32 i=0; i<v0; i++) { pPerm[ pI[i] ]= 0x20; }
                  for (U32 i=v0; i<v1; i++) { pPerm[ pI[i] ]= 0xF0; }
                  for (U32 i=v1; i<ve; i++) { pPerm[ pI[i] ]= 0x20; }
                  const char *name= "conn256u8.raw";
                  bytes= saveBuff(pPerm, name, org.n);
                  printf("%s %G%cBytes\n", name, binSizeZ(&m,bytes), m);
               }
            }
         }
      }
   }
   return(r);
} // mapFromU8Raw
