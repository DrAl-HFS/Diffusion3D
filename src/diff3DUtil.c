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

typedef struct
{
   U8 vMin,vMax;
} MMU8;

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

static size_t permTransMapU8 (U8 *pPerm, const U8 *pS, const size_t n, const float popF[2], const int maxPerm)
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
} // permTransMapU8

static size_t map6TransferU8 (D3S6MapElem * restrict pM, const U8 *pU8, const size_t n, const U8 t)
{
static const D3S6MapElem v6[2]= { (1<<6)-1, 0 };
   return( thresholdNU8(pM, pU8, n, t, v6) / v6[0] );
} // map6TransferU8

static size_t mapTransferU8 (D3MapElem * restrict pM, const U8 *pU8, const size_t n)
{
   const D3MapElem v26[2]= { -1, (1<<26)-1 };
   size_t s[2]={0,0};
   for (size_t i=0; i<n; i++)
   { 
      int c= (0 == pU8[i]);
      s[c]++;
      pM[i]= v26[c]; 
   }
   return( s[0] );
} // mapTransferU8

static void sealBoundaryMap (D3MapElem *pM, const MapOrg *pO, const D3MapElem extMask)
{
   for (Index y= pO->mm.vMin.y; y <= pO->mm.vMax.y; y++)
   {
      for (Index x= pO->mm.vMin.x; x <= pO->mm.vMax.x; x++)
      {
         const size_t i= x * pO->stride[0] + y * pO->stride[1] + pO->mm.vMin.z * pO->stride[2];
         const size_t j= x * pO->stride[0] + y * pO->stride[1] + pO->mm.vMax.z * pO->stride[2];
         pM[i]&= extMask|getBoundaryM26(x, y, pO->mm.vMin.z, &(pO->mm));
         pM[j]&= extMask|getBoundaryM26(x, y, pO->mm.vMax.z, &(pO->mm));
      }
   }
   for (Index z= pO->mm.vMin.z; z <= pO->mm.vMax.z; z++)
   {
      for (Index x= pO->mm.vMin.x; x <= pO->mm.vMax.x; x++)
      {
         const size_t i= x * pO->stride[0] + pO->mm.vMin.y * pO->stride[1] + z * pO->stride[2];
         const size_t j= x * pO->stride[0] + pO->mm.vMax.y * pO->stride[1] + z * pO->stride[2];
         pM[i]&= extMask|getBoundaryM26(x, pO->mm.vMin.y, z, &(pO->mm));
         pM[j]&= extMask|getBoundaryM26(x, pO->mm.vMax.y, z, &(pO->mm));
      }
      for (Index y= pO->mm.vMin.y; y <= pO->mm.vMax.y; y++)
      {
         const size_t i= pO->mm.vMin.x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
         const size_t j= pO->mm.vMax.x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
         pM[i]&= extMask|getBoundaryM26(pO->mm.vMin.x, y, z, &(pO->mm));
         pM[j]&= extMask|getBoundaryM26(pO->mm.vMax.x, y, z, &(pO->mm));
      }
   }
} // sealBoundaryMap

void dumpDistBC (const D3MapElem * pM, const size_t nM)
{
   size_t d1[27], d2[7], s= 0;
   for (int i=0; i<=26; i++) { d1[i]= 0; }
   for (int i=0; i<=6; i++) { d2[i]= 0; }
   for (size_t i=0; i<nM; i++)
   {
      uint b= bitCountZ( pM[i] & ((1<<26)-1) );
      d1[b]++;
      b= bitCountZ( pM[i] >> 26 );
      d2[b]++;
   }
   printf("BitCountDist: ");
   s= 0;
   for (int i=0; i<=26; i++) { printf("%zu ", d1[i]); s+= d1[i]; }
   printf("\nsum=%zu\nExtDist: ",s);
   s= 0;
   for (int i=0; i<=6; i++) { printf("%zu ", d2[i]); s+= d2[i]; }
   printf("\nsum=%zu\n: ",s);
} // dumpDistBC

void dumpDMMBC (const U8 *pU8, const D3MapElem * pM, const size_t n, const uint mask)
{
   size_t d[33], t= 0;
   MMU8 mm[33];
   for (int i=0; i<=32; i++) { mm[i].vMin= 0xFF; mm[i].vMax= 0; d[i]= 0; }
   for (size_t i=0; i < n; i++)
   {
      uint b= bitCountZ( pM[i] & mask );
      d[b]++;
      t+= ((0 == b) && (pU8[i] > 0));
      mm[b].vMin= MIN(mm[b].vMin, pU8[i]);
      mm[b].vMax= MAX(mm[b].vMax, pU8[i]);
   }
   printf("anomalous=%d\n", t);
   for (int i=0; i<=32; i++)
   {
      if (mm[i].vMax >= mm[i].vMin) { printf("%2u: %8zu %3u %3u\n", i, d[i], mm[i].vMin, mm[i].vMax); }
   }
} // dumpDMMBC

void checkComb (const V3I v[2], const Stride stride[3], const D3MapElem * pM)
{
   for (int k=0; k<2; k++)
   {
      for (int j=0; j<2; j++)
      {
         for (int i=0; i<2; i++)
         {
            const D3MapElem m= pM[ dotS3(v[i].x, v[j].y, v[k].z, stride) ];
            U8 r= m >> 26;
            uint m6= m & ((1<<6)-1);
            uint m12= (m>>6) & ((1<<12)-1);
            uint m8= (m>>18) & ((1<<8)-1);
            printf("(%3d,%3d,%3d) : %u ", v[i].x, v[j].y, v[k].z, r);
            printf("m: 0x%x(%u) 0x%x(%u) 0x%x(%u) ", m6, bitCountZ(m6), m12, bitCountZ(m12), m8, bitCountZ(m8));
            dumpM6(m,"\n"); 
         }
      }
   }
} // checkComb

static void constrainNH (D3MapElem * restrict pM, const U8 *pU8, const size_t n, 
                     const Stride step[], const D3MapElem revM[], const U8 nNH)
{
   size_t badR= 0;
   const D3MapElem nhM= (1 << nNH) - 1;
   for (size_t i= 0; i<n; i++)
   {
      U8 r= pU8[i];
      if ((0 == r) ^ (0 == (pM[i] & nhM) ))
      {
         if (0 == r)
         {
            for (U8 j=0; j < nNH; j++)
            {  // adjust all available neighbours, preventing flux into site
               if (pM[i] & (1<<j)) { pM[ i + step[j] ] &= revM[j]; }
            }
            pM[i]= 0;
         }
         else { badR++; }
      }
   }
   if (badR > 0) { printf("constrainNH() - badR=%zu\n", badR); }
} // constrainNH

static void genRevM (D3MapElem revM[], char n)
{
   const char revD[2]={+1,-1}; // even +1, odd -1 : 0->1, 1->0, 2->3, 3->2 ...
   // build reverse mask table
   for (int i=0; i < n; i++)
   {
      revM[i]= ~( 1 << ( i + revD[ (i&1) ] ) );
   }
} // genRevM

static void constrainMapNH (D3MapElem * const pM, const U8 * pU8, const MapOrg *pO, const U8 nNH)
{
   D3MapElem revM[26];

   if (nNH <= 26)
   {
      genRevM(revM,nNH);
      constrainNH(pM, pU8, pO->n, pO->step, revM, nNH);
   }
} // constrainMapNH

float processMap (D3MapElem * pM, V3I * pV, const U8 * pPerm, const MapOrg * pO)
{
   size_t r=-1, s;

   printf("transfer...\n");
   s= mapTransferU8(pM, pPerm, pO->n);
   dumpDMMBC(pPerm, pM, pO->n, -1);

   printf("seal...\n");
   sealBoundaryMap(pM, pO, gExtMask);
   dumpDMMBC(pPerm, pM, pO->n, (1<<26)-1);

   printf("constrain...\n");
   constrainMapNH(pM, pPerm, pO, 26);
   dumpDMMBC(pPerm, pM, pO->n, (1<<26)-1);

   if (pV)
   {
      V3I i, c;
      size_t mR=-1, mS=0;
      static const U8 v2[2]={0,1};
      MMV3I mm;
      I32 zS=-1;
      U8 t= 1;

      c.x= pO->def.x / 2;
      c.y= pO->def.y / 2;
      c.z= pO->def.z / 2;

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
                  size_t d= d2I3( i.x - c.x, i.y - c.y, i.z - c.z );
                  if (d < r) { r= d; *pV= i; t= pPerm[j]; }
               }
            }
         }
      }
   }
   return((float)s / pO->n );
} // processMap

static void step6FromStride (Index step[6], const Index stride[3])
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

size_t initDiffOrg (DiffOrg *pO, uint def, uint nP)
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

DiffScalar initIsoW (DiffScalar w[], DiffScalar r, uint nHood, uint f)
{
   DiffScalar t= 0;
   uint n[3]= {6,0,0};

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

float setDefaultMap (D3MapElem *pM, const V3I *pD, const uint id)
{
   MapOrg org;
   size_t n= initMapOrg(&org, pD);
   const D3MapElem me= getBoundaryM26V(org.def.x/2, org.def.y/2, org.def.z/2, &(org.mm));

   for (size_t i=0; i < n; i++) { pM[i]= me; }
   sealBoundaryMap(pM, &org, 0);

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
#if 1
   size_t t= 0;
   for (size_t i=0; i < n; i++) { t+= bitCountZ(pM[i] ^ me); }
   printf("setDefaultMap() - %zu\n",t);
#endif
   return(1);
} // setDefaultMap

Bool32 getProfileRM (RawTransMethodDesc *pRM, U8 id, U8 f)
{
   pRM->flags= f;
   pRM->nHood= 26;
   switch(id)
   {
      case 1 : pRM->method= 1; pRM->maxPermLvl= 1; 
         pRM->param[0]= 100; pRM->param[0]= 0.0; 
         break;
      case 2 : pRM->method= 2; pRM->maxPermLvl= (1<<3)-1; 
         pRM->param[0]= 0.35; pRM->param[1]= 0.0;//15; 
         break;
      default : return(FALSE);
   }
   return(TRUE);
} // getProfileRM

float mapFromU8Raw (D3MapElem *pM, RawTransInfo *pRI, const MemBuff *pWS, const char *path, 
      const RawTransMethodDesc *pRM, const DiffOrg *pO)
{
   float r=0;
   size_t bytes= fileSize(path);
   char m;
   MapOrg org;

   memset(pRI, 0, sizeof(*pRI));
   //nHood, maxPermSet, permAlign;
   if (initMapOrg(&org, &(pO->def)) > 0)
   {
      U8 *pRaw= pWS->p;
      bytes= loadBuff(pRaw, path, MIN(bytes, pWS->bytes));
      printf("mapFromU8Raw() - %G%cBytes\n", binSize(&m, bytes), m);
      if (bytes >= org.n)
      {
         U8 *pPerm= pRaw;

         switch(pRM->method)
         {
            case 1 :
            {
               const U8 v2[2]={1,0};
               if (thresholdNU8(pPerm, pRaw, org.n, pRM->param[0], v2) > 0) { pRI->maxPermSet= 1; }
               break;
            }
            case 2 :
               permTransMapU8(pPerm, pRaw, org.n, pRM->param, pRM->maxPermLvl);
               pRI->maxPermSet= pRM->maxPermLvl; // HACK!
               break;
         }
         r= processMap(pM, &(pRI->site), pPerm, &org);
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
            printf("%s %G%cBytes\n", name, binSize(&m,bytes), m);
         }
      }
   }
   return(r);
} // mapFromU8Raw
