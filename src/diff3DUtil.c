// diff3DUtil.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-June 2019

#include "diff3DUtil.h"
#include "clusterMap.h"


const D3MapElem gExtMask= 0x3f << 26;


/***/

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
   int i= 0;//REPORT(TRC,"%d:%zu\n", i, s);
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
      REPORT(LOG1,"popT%d/%d %G -> %d\n", iF, nF, f, t[iF]);
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
      TRACE("perm %Gx+%G\n", lm[0],lm[1]);
      int v[2]= { iT[0]*lm[0]+lm[1], iT[1]*lm[0]+lm[1] };
      TRACE(" %d->%d %d->%d\n", iT[0],v[0],iT[1],v[1]);
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


static size_t consNH32 (D3MapElem * const pM, const U8 nNH, const U8 * const pPermU8, const size_t n,
                     const Stride step[], const D3MapElem revM[])
{
   const D3MapElem nhM= (1 << nNH) - 1;
   size_t w=0;
   for (size_t i= 0; i<n; i++)
   {
      if (0 == pPermU8[i])
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
} // consNH32

static size_t consNHB (void * const pM, const U8 nMB, const U8 nNH, const U8 * const pPermU8, const size_t n,
                     const Stride step[], const D3MapElem revM[])
{
   const D3MapElem nhM= (1 << nNH) - 1;
   size_t w=0;
   for (size_t i= 0; i<n; i++)
   {
      if (0 == pPermU8[i])
      {
         const size_t iB= i * nMB;
         const D3MapElem m= readBytesLE(pM, iB, nMB);
         w+= (0 == (m & nhM)); if (0 == (m & nhM)) { WARN("***%zu?\n\n", i); }
         for (U8 j=0; j < nNH; j++)
         {  // adjust all available neighbours, preventing flux into site
            if (m & (1<<j)) { andBytesLE(pM, i + step[j], nMB, revM[j]); }
         }
         writeBytesLE(pM, iB, nMB, 0);
      }
   }
   return(w);
} // consNHB

static void genRevM (D3MapElem revM[], char n)
{
   const char revD[2]={+1,-1}; // even +1, odd -1 : 0->1, 1->0, 2->3, 3->2 ...
   // build reverse mask table
   for (int i=0; i < n; i++)
   {
      revM[i]= ~( 1 << ( i + revD[ (i&1) ] ) );
   }
} // genRevM

static void consMapNHX (void * const pM, const U8 nMB, const U8 nNH, const U8 * pPermU8, const MapOrg * pO)
{
   D3MapElem revM[26];

   if (nNH > (nMB<<3)) { ERROR("constrainMapNH() - nNH=%u, nMB=%u\n", nNH, nMB); }
   if (nNH <= 26)
   {
      size_t w=0;
      genRevM(revM,nNH);
      if (4 == nMB) { w= consNH32(pM, nNH, pPermU8, pO->n, pO->step, revM); }
      else { w= consNHB(pM, nMB, nNH, pPermU8, pO->n, pO->step, revM); }
      if (w > 0) { WARN("constrainMapNH() - %zu degenerate map entries\n", w);}
   }
} // consMapNHX

//#include "diffTestUtil.h"

float processMap (void * pM, const U8 nMapBytes, const U8 nNHBits, const U8 * pPerm, const MapOrg * pO, U8 v)
{
   size_t pd[256]={0,};

   TRACE("%s...\n","transfer");
   permTransferMapNH(pM, pd, nMapBytes, nNHBits, 8*nMapBytes-nNHBits, pPerm, pO->n);
   if (v > 0)
   {
      float r= 100.0 / pO->n;
      TRACE("%s\n","PermDist:");
      for (int i=0; i<256; i++)
      {
         if (pd[i] > 0) { REPORT(TRC1,"%d: %12zu = %G%%\n", i, pd[i], r * pd[i]); }
      }
      REPORT(TRC1,"\n",0);
      //if (4 == nMapBytes) { dumpDMMBC(pPerm, pM, pO->n, -1); }
   }
   TRACE("%s...\n","seal");
   sealBoundaryMapNH(pM, nMapBytes, pO, gExtMask);
   //if ((v > 0) && (4 == nMapBytes)) { dumpDMMBC(pPerm, pM, pO->n, (1<<nNHBits)-1); }

   TRACE("%s...\n","constrain");
   consMapNHX(pM, nMapBytes, nNHBits, pPerm, pO);
   //if ((v > 0) && (4 == nMapBytes)) { dumpDMMBC(pPerm, pM, pO->n, (1<<nNHBits)-1); }

   return((float)(pO->n - pd[0]) / pO->n );
} // processMap

size_t initDiffOrg (DiffOrg *pO, const U16 def[3], U32 nP)
{
   size_t n= 1;

   //def= MAX(32,def);
   //nP= MAX(1,nP);
   pO->def.x= def[0];
   pO->def.y= def[1];
   pO->def.z= def[2];
   pO->nPhase= nP;
   for (int i=0; i<DIFF_DIM; i++)
   {
      pO->stride[i]= n; // planar
      n*= def[i];
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
   if (f & 1) { TRACE("initIsoW() - w[]= %G, %G, %G, %G\n", w[0], w[1], w[2], w[3]); }
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
         //REPORT(TRC,"adjustMap26() - %zu\n", adjustMap26(pM, &org) );
#endif
         break;
      }
   }
   pMD->mapElemBytes=4; pMD->nHood=26; pMD->maxPermSet=0; pMD->permAlign= 26;
#if 1
   size_t t= 0;
   for (size_t i=0; i < n; i++) { t+= bitCountZ(pM[i] ^ me); }
   TRACE("setDefaultMap() - %zu\n",t);
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


float mapFromU8Raw (void *pM, MapDesc *pMD, const MemBuff *pWS, const char *path,
      const RawTransMethodDesc *pRM, const DiffOrg *pO)
{
   size_t bytes, n;
   float r=0;
   char ch[4];
   MapOrg org;
   U8 verbose=1;

   if (path) { bytes= fileSize(path); } else { bytes= pO->n1F; }
   if (pMD) { memset(pMD, 0, sizeof(*pMD)); }
   n= initMapOrg(&org, &(pO->def));
   if (validMemBuff(pWS,bytes) && (n > 0))
   {
      const V3I c= {org.def.x / 2, org.def.y / 2, org.def.z / 2};
      U8 *pRaw= pWS->p;

      if (path)
      {
         bytes= loadBuff(pRaw, path, MIN(bytes, pWS->bytes)); ch[3]= 'L';
         pRaw= pWS->p;
      }
      else { memcpy(pRaw, pM, bytes); ch[3]= 'C'; }
      TRACE("mapFromU8Raw() - {%c} %G%cBytes\n", ch[3], binSizeZ(ch, bytes), ch[0]);
      if (bytes >= org.n)
      {
         U8 *pPerm= pRaw;
         pMD->nHood= pRM->nBNH & 0x1F;
         pMD->mapElemBytes= pRM->nBNH >> 5;
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
         r= processMap(pM, pMD->mapElemBytes, pMD->nHood, pPerm, &org, verbose);
         snprintf(pMD->msg,sizeof(pMD->msg)-1,"processMap() - (%d,%d,%d) %G", org.def.x, org.def.y, org.def.z, r);
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
            TRACE("%s %G%cBytes\n", name, binSizeZ(ch,bytes), ch[0]);
         }
         //REPORT(TRC,"pRM->flags=0x%02x\n", pRM->flags);
         if (pRM->flags & D3UF_CLUSTER_TEST)
         {
            MemBuff ws;
            ClustRes r;
            U8 v[2]={0,1};

            thresholdNU8(pPerm, pPerm, org.n, 0, v); // (perm>0) -> 1
            adjustMemBuff(&ws, pWS, org.n, 0); // * sizeof(*pPerm);
            clusterExtract(&r, &ws, pPerm, &(org.def), org.stride);
            bytes= r.nNI * sizeof(*(r.pNI)) + r.nNC * sizeof(*(r.pNC));
            TRACE("Total: %G%cBytes\n", binSizeZ(ch,bytes), ch[0]);
            if (r.iNCMax > 0)
            {
               MMU32 maxC;
               size_t nMaxNI= clusterResGetMM(&maxC, &r, r.iNCMax);
               U32 ve= r.pNC[r.nNC];
               U32 *pMaxNI= r.pNI + maxC.vMin;
               U32 midN= findNIMinD(pMaxNI, nMaxNI, dotS3(c.x, c.y, c.z, org.stride));

               bytes= nMaxNI * sizeof(*(r.pNI));
               TRACE("Max: C%u : %u (%G%%) %G%cBytes\n", r.iNCMax, nMaxNI, nMaxNI * 100.0 / org.n, binSizeZ(ch,bytes), ch[0]);
               //pC[iM-1] + s >> 1); // median
               split3(&(pMD->site.x), pMaxNI[midN], org.stride);
               TRACE("Mid: [%u] : %u -> (%d,%d,%d)\n", midN, r.pNI[midN], pMD->site.x, pMD->site.y, pMD->site.z);

               if (pRM->flags & D3UF_CLUSTER_SAVE)
               {
                  const char *name= "conn256u8.raw";
                  for (U32 i= 0; i < maxC.vMin; i++) { pPerm[ r.pNI[i] ]= 0x20; }
                  for (U32 i= maxC.vMin; i < maxC.vMax; i++) { pPerm[ r.pNI[i] ]= 0xF0; }
                  for (U32 i= maxC.vMax; i<ve; i++) { pPerm[ r.pNI[i] ]= 0x20; }
                  bytes= saveBuff(pPerm, name, org.n);
                  TRACE("%s %G%cBytes\n", name, binSizeZ(ch,bytes), ch[0]);
               }

               if (1 == pMD->mapElemBytes)
               {  // move dominant cluster indices to end, for buffer reuse (discarding perm data)
                  bytes= sizeof(*pMaxNI) * nMaxNI;
                  void *p= (void*)(ws.w + ws.bytes - bytes);
                  memset(&r, 0, sizeof(r));
                  memmove(p, pMaxNI, bytes); pMaxNI= p;
                  adjustMemBuff(&ws, pWS, 0, bytes);
                  clusterMapTest(ws, pMaxNI, nMaxNI, pM, &org);
               } else { WARN("offsetMapTest() - does not support mapBytes=%u\n", pMD->mapElemBytes); }
            }
         }
         else if (0 == findInnoc(&(pMD->site), pPerm, &org) )
         {
            findNear(&(pMD->site), &c, pPerm, &org);
         }
      }
   } else { snprintf(pMD->msg,sizeof(pMD->msg)-1,"mapFromU8Raw() - MORG=%zu; %zu >= %zu -> %d\n", n, pWS->bytes, bytes, pWS->bytes >= bytes); }
   return(r);
} // mapFromU8Raw

static size_t consU8 (U8 * const pM, const U8 nNH, const U8 * const pRefM, const size_t n,
                     const Stride step[], const D3MapElem revM[])
{
   size_t w=0;
   for (size_t i= 0; i<n; i++)
   {
      if ((0 == pM[i]) && (0 != pRefM[i]))
      {
         const D3MapElem m= pRefM[i];
         for (U8 j=0; j < nNH; j++)
         {  // adjust all available neighbours, preventing flux into site
            if (m & (1<<j)) { pM[ i + step[j] ] &= revM[j]; }
         }
         ++w;
      }
   }
   return(w);
} // consU8

size_t constrainMap (void * pM, const void * pW, const MapDesc * pMD, const DiffOrg *pO)
{
   D3MapElem revM[26];
   if (1 == pMD->mapElemBytes)
   {
      MapOrg mo;
      initMapOrg(&mo, &(pO->def));
      genRevM(revM, pMD->nHood);
      return consU8(pM, pMD->nHood, pW, mo.n, mo.step, revM);
   }
   return(0);
} // constrainMap

size_t map8DupCons
(
   D3S6MapElem    * pR,     // result constrained map
   const D3S6MapElem * pM, // reference map
   const MapDesc  * pMD,    // map props
   const DiffOrg  * pO,     // scalar props
   const DiffScalar * pS0,  // scalar field
   const DiffScalar * pS1,  // scalar field
   const DupConsParam * pP  // threshold >=
)
{
   size_t k=0, l=0, m= 0, n= 0;
   DiffScalar s, t=0;

   for (size_t i=0; i<pO->n1F; i++)
   {
      s= pS0[i] + pS1[i];
      t+= s;
      if ((s >= pP->t[1]) && (0x00 == pR[i])) { pR[i]= pM[i]; ++n; }
      else if ((s <= pP->t[0]) && (0x00 != pR[i])) { pR[i]= 0x00; ++m; }
   }
   l= n + m;
   if (0 != pP->logLvl)
   {
      k= (size_t)(pP->logLvl & 0xF) << (pP->logLvl >> 4);
      if (l >= k)
      {
         LOG_CALL("() - sum=%G, +n=%zu, -m=%zu\n", t, n, m);
      }
   }
   if ((l > 0) || (pP->flags & 0x1)) { l= constrainMap(pR, pM, pMD, pO); }
   return(l);
} // map8DupCons
