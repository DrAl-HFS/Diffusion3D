// diff3DUtil.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diff3DUtil.h"

typedef struct
{
   V3I   def;
   Index stride[3];
   Index step[26];
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

static size_t procMapU8 (U8 *pR, const U8 *pS, size_t n, const U8 t, const U8 v[2])
{
   size_t s=0;
   for (size_t i= 0; i<n; i++) { s+= pR[i]= v[ (pS[i] > t) ]; }
   return(s);
} // procMapU8

static int findIdxT (const size_t h[], int maxH, size_t t)
{
   size_t s=0;
   int i= 0;
   do { s+= h[i]; i+= (s < t); } while ((s < t) && (i < maxH));
   return(i);
} // findIdxT

static int popT (const U8 *pS, const size_t n, double popF)
{
   size_t h[256]={0};
   for (size_t i= 0; i<n; i++) { h[ pS[i] ]++; }
   if ((popF > 0) && (popF <= 1)) { popF*= n; }
   return findIdxT(h, 256, popF);
} // popT

static size_t permTransMapU8 (U8 *pR, const U8 *pS, const size_t n, const float f, const int maxPerm)
{
   size_t s= 0;
   const int iT= popT(pS, n, f);

   if ((iT > 0) && (iT < 256))
   {
      double lm[2];
      lm[0]= (maxPerm - 0) / (0 - iT);
      lm[1]= maxPerm;
      for (size_t i= 0; i<n; i++)
      {
         int v= lm[0] * pS[i] + lm[1]; 
         if (v >= 1) { pR[i]= MIN(v,maxPerm); s++; } else { pR[i]= 0; }
      }
   }
   return(s);
} // permTransMapU8

static size_t map6TransferU8 (D3S6MapElem * restrict pM, const U8 *pU8, const size_t n, const U8 t)
{
static const D3S6MapElem v6[2]= { (1<<6)-1, 0 };
   return( procMapU8(pM, pU8, n, t, v6) / v6[0] );
} // map26TransferU8

static size_t mapTransferU8 (D3MapElem * restrict pM, const U8 *pU8, const size_t n, const U8 t)
{
   const D3MapElem v26[2]= { -1, (1<<26)-1 };
   size_t s[2]={0,0};
   for (size_t i=0; i<n; i++)
   { 
      int c= (pU8[i] > t);
      s[c]++;
      pM[i]= v26[c]; 
   }
   printf("mapTransferU8() - v26[]=0x%x,0x%x  t=%u %zu %zu %zu\n", v26[0], v26[1], t, s[0], s[1], s[0]+s[1]);
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

static void genRevM (D3MapElem revM[], char n)
{
   const char revD[2]={+1,-1}; // even +1, odd -1 : 0->1, 1->0, 2->3, 3->2 ...
   // build reverse mask table
   for (int i=0; i < n; i++)
   {
      revM[i]= ~( 1 << ( i + revD[ (i&1) ] ) );
   }
} // genRevM

static void adjustMapN (D3MapElem * const pM, const MapOrg *pO, const char n, const D3MapElem extMask)
{
   D3MapElem revM[26];//, u=0;
   const MMV3I *pMM= &(pO->mm);

   genRevM(revM,n);
   for (Index z= pMM->vMin.z; z < pMM->vMax.z; z++)
   {
      for (Index y= pMM->vMin.y; y < pMM->vMax.y; y++)
      {
         for (Index x= pMM->vMin.x; x < pMM->vMax.x; x++)
         {
            const size_t i= dotS3(x,y,x,pO->stride);
            if (0 == (pM[i] & extMask))
            {  // obstruction
               for (char j=0; j < n; j++)
               {  // adjust all available neighbours, preventing flux into site
                  if (pM[i] & (1<<j)) { pM[i + pO->step[j] ] &= revM[j]; }
               }
               pM[i]= 0; // prevent processing of obstructed sites
            }
         }
      }
   }
} // adjustMapN

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

void dumpDMMBC (const U8 *pU8, const D3MapElem * pM, const size_t n)
{
   size_t d[33];
   MMU8 mm[33];
   for (int i=0; i<=32; i++) { mm[i].vMin= 0xFF; mm[i].vMax= 0; d[i]= 0; }
   for (size_t i=0; i < n; i++)
   {
      uint b= bitCountZ( pM[i] );
      d[b]++;
      mm[b].vMin= MIN(mm[b].vMin, pU8[i]);
      mm[b].vMax= MAX(mm[b].vMax, pU8[i]);
   }
   for (int i=0; i<=32; i++)
   {
      if (mm[i].vMax >= mm[i].vMin) { printf("%2u: %8zu %3u %3u\n", i, d[i], mm[i].vMin, mm[i].vMax); }
   }
} // dumpDMMBC

void repairHack (D3MapElem * restrict pM, const U8 *pU8, const size_t n, const U8 t)
{
   D3MapElem m[]={ -1, 0 };
   for (size_t i= 0; i<n; i++) { pM[i]&= m[ (pU8[i] > t) ]; }
} // repairHack

float processMap (D3MapElem * pM, MapSiteInfo * pMSI, U8 * pU8, const MapOrg * pO, U8 t)
{
   size_t r=-1, s;

   printf("transfer...\n");
   s= mapTransferU8(pM, pU8, pO->n, t );
   dumpDMMBC(pU8, pM, pO->n);

   printf("seal...\n");
   sealBoundaryMap(pM, pO, gExtMask);
   dumpDMMBC(pU8, pM, pO->n);

   printf("adjust...\n");
   adjustMapN(pM, pO, 26, gExtMask);
   //repairHack(pM, pU8, pO->n, t);
   dumpDMMBC(pU8, pM, pO->n);

   //dumpDistBC(pM, pO->n);
   //printf("processMap() - %zu %zu\n", s, pO->n);

   if (pMSI)
   {
      V3I i, c, m;
      size_t mR=-1, mS=0;
      static const U8 v2[2]={0,1};
      MMV3I mm;
      I32 zS=-1;

      procMapU8(pU8, pU8, pO->n, t, v2);

      c.x= pO->def.x / 2;
      c.y= pO->def.y / 2;
      c.z= pO->def.z / 2;

      adjustMMV3I(&mm, &(pO->mm), -1);
      pMSI->c= c;
      i.z= c.z;
      for (i.z=mm.vMin.z; i.z < mm.vMax.z; i.z++)
      {
         size_t s= 0, b= i.z * pO->stride[2];
         for (int j= 0; j < pO->stride[2]; j++) { s+= (0 == pU8[b+j]); }
         if (s > mS) { zS= i.z; }
         for (i.y=mm.vMin.y; i.y < mm.vMax.y; i.y++)
         {
            for (i.x=mm.vMin.x; i.x < mm.vMax.x; i.x++)
            {
               const size_t j= i.x * pO->stride[0] + i.y * pO->stride[1] + i.z * pO->stride[2];
               if (0 == pU8[j])
               {
                  size_t d= d2I3( i.x - c.x, i.y - c.y, i.z - c.z );
                  if (d < r) { r= d; pMSI->c= i; }
               }
            }
         }
      }
   }
   return((float)s / pO->n );
} // processMap

/* DEPRECATE
static D3MapElem conformantS26M (const D3MapElem *pM, const Index step[26])
{
   D3MapElem m= *pM;
   for (int i=0; i < 26; i++)
   {
      const uint t= 1 << i;
      if ((m & t) && (0 == pM[ step[i] ])) { m&= ~t; }
   }
   return(m);
} // conformantS26M

static size_t adjustMap26 (D3MapElem *pM, const MapOrg *pO)
{
   size_t n= 0;
   for (Index z= pO->mm.vMin.z; z < pO->mm.vMax.z; z++)
   {
      for (Index y= pO->mm.vMin.y; y < pO->mm.vMax.y; y++)
      {
         for (Index x= pO->mm.vMin.x; x < pO->mm.vMax.x; x++)
         {
            const size_t i= dotS3(x,y,x,pO->stride);
            pM[i]= conformantS26M(pM + i, pO->step);
            n++;
         }
      }
   }
   return(n);
} // adjustMap26
*/
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

   w[1]= w[2]= w[3]= 0;
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

float mapFromU8Raw (D3MapElem *pM, MapSiteInfo *pMSI, const MemBuff *pWS, const char *path, U8 t, const DiffOrg *pO)
{
   size_t bytes= fileSize(path);
   char m;
   MapOrg org;

   if (initMapOrg(&org, &(pO->def)) > 0)
   {
      bytes= loadBuff(pWS->p, path, MIN(bytes, pWS->bytes));
      printf("mapFromU8Raw() - %G%cBytes\n", binSize(&m, bytes), m);
      if (bytes >= org.n)
      {
         return processMap(pM, pMSI, pWS->p, &org, t);
      }
   }
   return(0);
} // mapFromU8Raw
