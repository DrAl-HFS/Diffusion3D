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
} MapOrg;


/***/

static void map6TransferU8 (D3S6MapElem * restrict pM, const U8 *pU8, const size_t n, const U8 t)
{
   for (size_t i=0; i<n; i++) { pM[i]= (pU8[i] <= t) ? ((1<<6)-1) : 0; }
} // map26TransferU8

static size_t map26TransferU8 (D3MapElem * restrict pM, const U8 *pU8, const size_t n, const U8 t)
{
   size_t nOpen=0;
   for (size_t i=0; i<n; i++)
   { 
      pM[i]= (pU8[i] <= t) ? ((1<<26)-1) : 0;
      nOpen+= (0 != pM[i]);
   }
   return(nOpen);
} // map26TransferU8

static D3MapElem conformantS26M (const D3MapElem *pM, Index step[26])
{
   D3MapElem m= *pM;
   for (int i=0; i < 26; i++)
   {
      const uint t= 1 << i;
      if ((m & t) && (0 == pM[ step[i] ])) { m&= ~t; }
   }
   return(m);
} // conformantS26M

static void adjustMap26 (D3MapElem *pM, const MapOrg *pO)
{
   for (Index z= pO->mm.min.z; z < pO->mm.max.z; z++)
   {
      for (Index y= pO->mm.min.y; y < pO->mm.max.y; y++)
      {
         for (Index x= pO->mm.min.x; x < pO->mm.max.x; x++)
         {
            const size_t i= x * pO->stride[0] + y * pO->stride[0] + z * pO->stride[1];
            pM[i]= conformantS26M(pM + i, pO->step);
         }
      }
   }
} // adjustMap26

static uint getBoundaryM6 (Index x, Index y, Index z, const MMV3I *pMM)
{
   uint m6= 0;

   m6|= (x > pMM->min.x) << 0;
   m6|= (x < pMM->max.x) << 1;

   m6|= (y > pMM->min.x) << 2;
   m6|= (y < pMM->max.y) << 3;

   m6|= (z > pMM->min.z) << 4;
   m6|= (z < pMM->max.z) << 5;

   return(m6);
} // getBoundaryM6

static uint getBoundaryM8 (const uint m6)
{
   uint m8= 0;

   m8|= ((m6 & 0x01) && (m6 & 0x04) && (m6 & 0x10)) << 0; // -X -Y -Z
   m8|= ((m6 & 0x01) && (m6 & 0x04) && (m6 & 0x20)) << 1; // -X -Y +Z
   m8|= ((m6 & 0x01) && (m6 & 0x08) && (m6 & 0x20)) << 2; // -X +Y +Z
   m8|= ((m6 & 0x01) && (m6 & 0x08) && (m6 & 0x10)) << 3; // -X +Y -Z

   m8|= ((m6 & 0x02) && (m6 & 0x04) && (m6 & 0x10)) << 4; // +X -Y -Z
   m8|= ((m6 & 0x02) && (m6 & 0x04) && (m6 & 0x20)) << 5; // +X -Y +Z
   m8|= ((m6 & 0x02) && (m6 & 0x08) && (m6 & 0x20)) << 6; // +X +Y +Z
   m8|= ((m6 & 0x02) && (m6 & 0x08) && (m6 & 0x10)) << 7; // +X +Y -Z

   return(m8);
} // getBoundaryM8

static uint getBoundaryM12 (const uint m6)
{
   uint m12= 0;

   m12|= ((m6 & 0x01) && (m6 & 0x04)) << 0; // -X -Y
   m12|= ((m6 & 0x01) && (m6 & 0x08)) << 1; // -X +Y
   m12|= ((m6 & 0x02) && (m6 & 0x04)) << 2; // +X -Y
   m12|= ((m6 & 0x02) && (m6 & 0x08)) << 3; // +X +Y

   m12|= ((m6 & 0x01) && (m6 & 0x10)) << 4; // -X -Z
   m12|= ((m6 & 0x01) && (m6 & 0x20)) << 5; // -X +Z
   m12|= ((m6 & 0x02) && (m6 & 0x10)) << 6; // +X -Z
   m12|= ((m6 & 0x02) && (m6 & 0x20)) << 7; // +X +Z

   m12|= ((uint)((m6 & 0x04) && (m6 & 0x10))) << 8; // -Y -Z
   m12|= ((uint)((m6 & 0x04) && (m6 & 0x20))) << 9; // -Y +Z
   m12|= ((uint)((m6 & 0x08) && (m6 & 0x10))) << 10; // +Y -Z
   m12|= ((uint)((m6 & 0x08) && (m6 & 0x20))) << 11; // +Y +Z

   return(m12);
} // getBoundaryM12

static uint getMapElem (Index x, Index y, Index z, const MMV3I *pMM)
{
   const uint m6= getBoundaryM6(x, y, z, pMM);
   return( m6 | (getBoundaryM12(m6) << 6) | (getBoundaryM8(m6) << 18) );
} // getMapElem

static uint getMapElemV (Index x, Index y, Index z, const MMV3I *pMM)
{
   const uint m6= getBoundaryM6(x, y, z, pMM);
   const uint m12= getBoundaryM12(m6);
   const uint m8= getBoundaryM8(m6);
   printf("getMapElemV(%d, %d, %d) - m6=0x%x, m12=0x%x, m8=0x%x\n", x, y, z, m6, m12, m8);
   return(m6 | (m12 << 6) | (m8 << 18));
} // getMapElemV

static void sealBoundaryMap (D3MapElem *pM, const MapOrg *pO)
{
   for (Index y= pO->mm.min.y; y <= pO->mm.max.y; y++)
   {
      for (Index x= pO->mm.min.x; x <= pO->mm.max.x; x++)
      {
         const size_t i= x * pO->stride[0] + y * pO->stride[1] + pO->mm.min.z * pO->stride[2];
         const size_t j= x * pO->stride[0] + y * pO->stride[1] + pO->mm.max.z * pO->stride[2];
         pM[i]&= getMapElem(x, y, pO->mm.min.z, &(pO->mm));
         pM[j]&= getMapElem(x, y, pO->mm.max.z, &(pO->mm));
      }
   }
   for (Index z= pO->mm.min.z; z <= pO->mm.max.z; z++)
   {
      for (Index x= pO->mm.min.x; x <= pO->mm.max.x; x++)
      {
         const size_t i= x * pO->stride[0] + pO->mm.min.y * pO->stride[1] + z * pO->stride[2];
         const size_t j= x * pO->stride[0] + pO->mm.max.y * pO->stride[1] + z * pO->stride[2];
         pM[i]&= getMapElem(x, pO->mm.min.y, z, &(pO->mm));
         pM[j]&= getMapElem(x, pO->mm.max.y, z, &(pO->mm));
      }
      for (Index y= pO->mm.min.y; y <= pO->mm.max.y; y++)
      {
         const size_t i= pO->mm.min.x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
         const size_t j= pO->mm.max.x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
         pM[i]&= getMapElem(pO->mm.min.x, y, z, &(pO->mm));
         pM[j]&= getMapElem(pO->mm.max.x, y, z, &(pO->mm));
      }
   }
} // sealBoundaryMap

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

   pO->mm.min.x= pO->mm.min.y= pO->mm.min.z= 0;
   pO->mm.max.x= pO->def.x-1;
   pO->mm.max.y= pO->def.y-1;
   pO->mm.max.z= pO->def.z-1;

   return((size_t)pO->stride[2] * pO->def.z);
} // initMapOrg

Bool32 initDiffOrg (DiffOrg *pO, uint def, uint nP)
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
   return(n > 0);
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

size_t setDefaultMap (D3MapElem *pM, const V3I *pD)
{
   MapOrg org;
   size_t n= initMapOrg(&org, pD);
   const D3MapElem me= getMapElemV(org.def.x/2, org.def.y/2, org.def.z/2, &(org.mm));

   for (size_t i=0; i < n; i++) { pM[i]= me; }
   sealBoundaryMap(pM, &org);
#if 1
   size_t t= 0;
   for (size_t i=0; i < n; i++) { t+= bitCountZ(pM[i] ^ me); }
   printf("setDefaultMap() - %zu\n",t);
#endif
   return(n);
} // setDefaultMap

size_t d2I3 (int dx, int dy, int dz) { return( dx*dx + dy*dy + dz*dz ); }

size_t mapFromU8Raw (D3MapElem *pM, MapInfo *pMI, const MemBuff *pWS, const char *path, U8 t, const DiffOrg *pO)
{
   size_t bytes= fileSize(path);
   char m;
   MapOrg org;
   size_t n= initMapOrg(&org, &(pO->def));

   bytes= loadBuff(pWS->p, path, MIN(bytes, pWS->bytes));
   printf("mapFromU8Raw() - %G%cBytes\n", binSize(&m, bytes), m);
   if (bytes >= n)
   {
      size_t r=-1, o= map26TransferU8(pM, pWS->p, n, t);

      if (pMI)
      {
         V3I i, c, m;
         size_t r= -1;
         c.x= org.def.x / 2;
         c.y= org.def.y / 2;
         c.z= org.def.z / 2;
         pMI->m= c;

         i.z= c.z;
         for (i.y=org.mm.min.y; i.y < org.mm.max.y; i.y++)
         {
            for (i.x=org.mm.min.x; i.x < org.mm.max.x; i.x++)
            {
               const size_t j= i.x * org.stride[0] + i.y * org.stride[1] + i.z * org.stride[2];
               if (0 != pM[j])
               {
                  size_t d= d2I3( i.x - c.x, i.y - c.y, i.z - c.z );
                  if (d < r) { r= d; pMI->m= i; }
               }
            }
         }
      }

      sealBoundaryMap(pM, &org);
      adjustMap26(pM, &org);
      return(n);
   }
   return(0);
} // mapFromU8Raw
