// diff3DUtil.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diff3DUtil.h"


/***/

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
   return( m6 | (getBoundaryM12(m6) << 6) );
} // getMapElem

static uint getMapElemV (Index x, Index y, Index z, const MMV3I *pMM)
{
   const uint m6= getBoundaryM6(x, y, z, pMM);
   const uint m12= getBoundaryM12(m6);
   const uint m8= getBoundaryM8(m6);
   printf("getMapElemV() - m6=0x%x, m12=0x%x, m8=0x%x\n", m6, m12, m8);
   return(m6 | (m12 << 6) );
} // getMapElemV

size_t setDefaultMap (D3MapElem *pM, const V3I *pD)
{
   const size_t nP= pD->x * pD->y;
   const size_t nV= nP * pD->z;
   const Stride s[2]= {pD->x,nP};
   const MMV3I mm= { 0, 0, 0, pD->x-1, pD->y-1, pD->z-1 };
   Index x, y, z;
   const D3MapElem me= getMapElemV(pD->x/2, pD->y/2, pD->z/2, &mm);

   for (z=1; z < pD->z-1; z++)
   {
      for (y=1; y < pD->y-1; y++)
      {
         for (x=1; x < pD->x-1; x++)
         {
            pM[x + y * s[0] + z * s[1]]= me;
         }
      }
   }
   z= pD->z-1;
   for (y=0; y < pD->y; y++)
   {
      for (x=0; x < pD->x; x++)
      {
         pM[x + y * s[0] + 0 * s[1]]= getMapElem(x,y,0,&mm);
         pM[x + y * s[0] + z * s[1]]= getMapElem(x,y,z,&mm);
      }
   }
   for (z=0; z < pD->z; z++)
   {
      y= pD->y-1;
      for (x=0; x < pD->x; x++)
      {
         pM[x + 0 * s[0] + z * s[1]]= getMapElem(x,0,z,&mm);
         pM[x + y * s[0] + z * s[1]]= getMapElem(x,y,z,&mm);
      }
      x= pD->x-1;
      for (y=0; y < pD->y; y++)
      {
         pM[0 + y * s[0] + z * s[1]]= getMapElem(0,y,z,&mm);
         pM[x + y * s[0] + z * s[1]]= getMapElem(x,y,z,&mm);
      }
   }
#if 1
   size_t t= 0;
   for (size_t i=0; i < nV; i++) { t+= pM[i] ^ me; }
   return(t);
#endif
} // setDefaultMap

Bool32 initOrg (DiffOrg *pO, uint def, uint nP)
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

   pO->step[0]= -pO->stride[0];
   pO->step[1]=  pO->stride[0];
   pO->step[2]= -pO->stride[1];
   pO->step[3]=  pO->stride[1];
   pO->step[4]= -pO->stride[2];
   pO->step[5]=  pO->stride[2];

   pO->n1P= pO->def.x * pO->def.y; // HACKY! check interleaving!
   pO->n1F= n;
   n*= pO->nPhase;
   pO->n1B= n;
   return(n > 0);
} // initOrg

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
