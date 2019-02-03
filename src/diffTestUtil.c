// diffTestUtil.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diffTestUtil.h"

typedef struct { DiffScalar x, y; } SearchPoint;

const float gEpsilon= 1.0 / (1<<30);


/***/


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

DiffScalar initW (DiffScalar w[], DiffScalar r, uint nHood, uint qBits)
{
   DiffScalar t= 0;
   uint n[3]= {6,0,0};
   w[1]= w[2]= w[3]= 0;
   if ((6 == nHood) && (qBits > 3))
   {
      const uint q= 1<<qBits;
      //switch (nHood){
      w[1]= r * (q / 6) / q; // ????
   }
   else
   {
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
   }
   t= (n[0] * w[1] + n[1] * w[2] + n[2] * w[3]);
   w[0]= 1 - t;
   printf("initW() - w[]= %G, %G, %G, %G\n", w[0], w[1], w[2], w[3]);
   return(t);
} // initW

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

void defFields (DiffScalar * pS, const DiffOrg *pO, DiffScalar v) // v[])
{
   const size_t b1B= pO->n1B * sizeof(*pS);
   size_t i;
   V3I c;
   uint phase;

   memset(pS, 0, b1B);

   c.x= pO->def.x/2; c.y= pO->def.y/2; c.z= pO->def.z/2;
   i= c.x * pO->stride[0] + c.y * pO->stride[1] + c.z * pO->stride[2];
   phase= 0;
   do
   {
      pS[i]= v;//[phase];
      i+= pO->phaseStride;
   } while (++phase < pO->nPhase);
} // defFields

float d2F3 (float dx, float dy, float dz) { return( dx*dx + dy*dy + dz*dz ); }

float setDiffIsoK (DiffScalar k[2], const DiffScalar Dt, const uint dim)
{
   const DiffScalar msd= 2 * Dt; // mean squared distance = variance
   k[0]= pow(2 * M_PI * msd, -0.5 * dim);
   k[1]= -0.5 / msd;
   return sqrt(msd);
} // setDiffIsoK

static DiffScalar compareAnalyticP (DiffScalar * restrict pTR, const DiffScalar * pS, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt)
{
   DiffScalar sad= 0;
   DiffScalar k[2];
   V3I c;

   #pragma acc data present( pTR[:pO->n1P], pO[:1] )
   {
      #pragma acc parallel loop vector
      for (Index i=0; i < pO->n1P; i++) { pTR[i]= 0; }
   }

   c.x= pO->def.x/2; c.y= pO->def.y/2; c.z= pO->def.z/2;
   setDiffIsoK(k, Dt, 3);

   #pragma acc data present( pTR[:pO->n1P], pS[:pO->n1F], pO[:1] ) \
                     copyin( k[:2], v, c )
   {
      for (Index z=0; z < pO->def.z; z++)
      {
         #pragma acc parallel loop
         for (Index y=0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x=0; x < pO->def.x; x++)
            {
               float r2= d2F3( x - c.x, y - c.y, z - c.z );
               DiffScalar w= v * k[0] * exp(r2 * k[1]);
               size_t j= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
               pTR[x + y * pO->def.x]+= fabs(pS[j] - w);
            }
         }
      }
   }

   #pragma acc data present( pTR[:pO->n1P], pO[:1] ) copy( sad )
   {
      #pragma acc parallel reduction(+: sad )
      for (Index i=0; i < pO->n1P; i++) { sad+= pTR[i]; }
   }
   return(sad);
} // compareAnalyticP

DiffScalar compareAnalytic (DiffScalar * restrict pTR, const DiffScalar * pS, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt)
{
   DiffScalar sad;
   #pragma acc data create( pTR[:pO->n1P] ) copyin( pS[:pO->n1F], pO[:1] )
   {
      sad= compareAnalyticP(pTR,pS,pO,v,Dt);
   }
   return(sad);
} // compareAnalytic

DiffScalar initPhaseAnalytic (DiffScalar * pR, const DiffOrg *pO, const uint phase, const DiffScalar v, const DiffScalar Dt)
{
   if (phase < pO->nPhase)
   {
      DiffScalar k[2], sigma, rm, r2m, t[3]= {0,0,0};
      V3I i, c;
      c.x= pO->def.x/2; c.y= pO->def.y/2; c.z= pO->def.z/2;

      sigma= setDiffIsoK(k, Dt, 3);
      rm= 4 * sigma;
      r2m= rm * rm;

      pR+= phase * pO->phaseStride;
      for (i.z=0; i.z < pO->def.z; i.z++)
      {
         for (i.y=0; i.y < pO->def.y; i.y++)
         {
            for (i.x=0; i.x < pO->def.x; i.x++)
            {
               float r2= d2F3( i.x - c.x, i.y - c.y, i.z - c.z );

               size_t j= i.x * pO->stride[0] + i.y * pO->stride[1] + i.z * pO->stride[2];
               pR[j]= v * k[0] * exp(r2 * k[1]);
               t[(r2 > r2m)]+= pR[j];
            }
         }
      }
      for (size_t j=0; j < pO->n1F; j++) { t[2]+= pR[j]; }
      printf("initPhaseAnalytic() - rm (4s) =%G, t=%G,%G,%G (%G)\n", rm, t[0], t[1], t[2], t[0]+t[1]);
      return(t[2]);
   }
   return(0);
} // initPhaseAnalytic

size_t saveSliceRGB (const char path[], const DiffScalar * pS, const uint phase, const uint z, const DiffOrg *pO)
{
   if ((phase < pO->nPhase) && (z < pO->def.z))
   {
      unsigned char *pB;
      const size_t n1P= pO->def.x * pO->def.y;
      size_t nB= n1P * 3;
      pB= malloc(nB);
      if (pB)
      {
         DiffScalar rm[2]={0,0}, mm[2]= {1E34,-1E34};
         pS+= phase * pO->phaseStride + z * pO->stride[2];
         for (size_t i=0; i < n1P; i++)
         {
            size_t j= i * pO->stride[0];
            mm[0]= MIN(mm[0], pS[j]);
            mm[1]= MAX(mm[1], pS[j]);
         }
         if (0 != mm[0]) { rm[0]= 1.0 / mm[0]; }
         if (0 != mm[1]) { rm[1]= 1.0 / mm[1]; }
         for (size_t i=0; i < n1P; i++)
         {
            size_t j= i * pO->stride[0];
            size_t k= i * 3;
            unsigned char r= 0, g=0, b= 0;
            if (pS[j] > gEpsilon)
            {
               float t= rm[1] * pS[j];
               g= 0x00 + t * 0xF0;
               b= 0x40 + t * 0xBF;
            }
            else if (pS[j] < 0)
            {
               float t= rm[0] * pS[j];
               g= 0x00 + t * 0xF0;
               r= 0x40 + t * 0xBF;
            }

            pB[k+0]= r;
            pB[k+1]= g;
            pB[k+2]= b;
         }
         nB= saveBuff(pB, path, nB);
         printf("saveSliceRGB(%s ) - (mm=%G,%G) %zu bytes\n", path, mm[0],mm[1], nB);
         free(pB);
         return(nB);
      }
   }
   return(0);
} // saveSliceRGB

DiffScalar sumStrideNS (const DiffScalar * pS, const size_t n, const Stride s)
{
   DiffScalar sum= 0;
   for (size_t i=0; i<n; i++) { sum+= pS[i * s]; }
   return(sum);
} // sumStrideNS

DiffScalar sumField (const DiffScalar * pS, const int phase, const DiffOrg *pO) // v[])
{
   if (phase < pO->nPhase) { return sumStrideNS(pS + pO->n1F * phase, pO->n1F, pO->stride[0]); }
   return(0);
} // size_t

SMVal diffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride sr12)
{
   StatMom  mom={0,0,0};
   StatRes1 sr={0,0};
   SMVal    sad=0;

   if (pR)
   {
      for (size_t i=0; i<n; i++)
      {
         DiffScalar s= pS1[i * sr12] + pS2[i * sr12]; 
         DiffScalar d= pS1[i * sr12] - pS2[i * sr12]; 
         pR[i]= d;
         sad+= fabs(d);
         if (0 != s)
         {
            mom.m[0]+= 1;
            mom.m[1]+= d;
            mom.m[2]+= d * d;
         }
      }
   }
   else
   {
      for (size_t i=0; i<n; i++)
      {
         DiffScalar s= pS1[i * sr12] + pS2[i * sr12]; 
         DiffScalar d= pS1[i * sr12] - pS2[i * sr12]; 
         sad+= fabs(d);
         if (0 != s)
         {
            mom.m[0]+= 1;
            mom.m[1]+= d;
            mom.m[2]+= d * d;
         }
      }
   }
   
   if (statGetRes1(&sr, &mom, 1))
   {
      printf("diffStrideNS() - n,sum,sumSqr,mean,var= %G, %G, %G, %G, %G\n", mom.m[0], mom.m[1], mom.m[2], sr.m, sr.v);
   }
   return(sad);
} // diffStrideNS

SMVal relDiffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride sr12)
{
   StatMom  mom={0,0,0};
   StatRes1 sr={0,0};
   SMVal    sad=0;

   for (size_t i=0; i<n; i++)
   {
      DiffScalar s1= pS1[i * sr12]; 
      DiffScalar s2= pS2[i * sr12]; 
      DiffScalar s= s1 + s2; 
      DiffScalar d= s1 - s2; 
      DiffScalar rd= 0;
      sad+= fabs(d);
      if (0 != s)
      {
         rd= (pS1[i * sr12] - s2) / s2;
         mom.m[0]+= 1;
         mom.m[1]+= d;
         mom.m[2]+= d * d;
      }
      pR[i]= rd;
   }
   if (statGetRes1(&sr, &mom, 1))
   {
      printf("diffStrideNS() - n,sum,sumSqr,mean,var= %G, %G, %G, %G, %G\n", mom.m[0], mom.m[1], mom.m[2], sr.m, sr.v);
   }
   return(sad);
} // relDiffStrideNS


DiffScalar searchMin1 (const MemBuff * pWS, const DiffScalar *pS, const DiffOrg *pO, const DiffScalar ma, const DiffScalar Dt)
{
   DiffScalar  *pTR= pWS->p;
   SearchPoint p, min[2];
   int r=2;

   #pragma acc data create( pTR[:pO->n1P] ) copyin( pS[:pO->n1F], pO[:1] ) copyout( pTR[:pO->n1P] )
   {
      //p.y=  compareAnalytic(pS, pO, ma, Dt);
      min[0].x= 0.5 * Dt;
      min[0].y= compareAnalyticP(pTR, pS, pO, ma, min[0].x);
      min[1].x= 1.5 * Dt;
      min[1].y= compareAnalyticP(pTR, pS, pO, ma, min[1].x);
      if (min[1].y < min[0].y) { SWAP(SearchPoint, min[0], min[1]); }
      printf("searchMin1() -\n  Dt   SAD\n? %G %G\n? %G %G\n", min[0].x, min[0].y, min[1].x, min[1].y);

      do
      {
         p.x= 0.5 * (min[0].x + min[1].x);
         p.y= compareAnalyticP(pTR, pS, pO, ma, p.x);
         printf("? %G %G\n", p.x, p.y);
         if (p.y < min[0].y)
         {
            r+= p.y < (min[0].y - gEpsilon);
            min[1]= min[0];
            min[0]= p;
         }
         else if (p.y < min[1].y)
         {
            r+= p.y < (min[1].y - gEpsilon);
            min[1]= p;
         }
      } while ((--r > 0) && ((min[1].y - min[0].y) > gEpsilon));
   }

   printf(": %G %G\n", min[0].x, min[0].y);
   return(min[0].x);
} // searchMin1

DiffScalar searchNewton (const MemBuff * pWS, const DiffScalar *pS, const DiffOrg *pO, const DiffScalar ma, const DiffScalar estDt)
{
   DiffScalar  *pTR= pWS->p;
   SearchPoint min={0,1E34};
   DiffScalar x, y[3], dy, hW= 0.0625 * estDt;
   const int iMax= 10;
   int i= 0, r= 0, m= 0;

   x= estDt;
   do
   {
      DiffScalar tx= x - hW;
      for (int j= 0; j<3; j++)
      {
         y[j]= compareAnalyticP(pTR, pS, pO, ma, tx);
         if (y[j] < min.y) { min.y= y[j]; min.x= tx; m++; }
         tx+= hW;
      }
      if (m > 0) { printf("min (%G %G)\n", min.x, min.y); m= 0; }
      tx= -1;
      dy= y[2] - y[0];
      if (fabs(dy) > gEpsilon)
      {
         DiffScalar dx= 2 * hW;
         dx= y[1] * dx / dy; // f(x) / (dy/dx)
         tx= x - dx;
         DiffScalar t= fabs(dx);
         if (t <= hW) { hW= 0.5 * t; }
      } 
      printf("%G %G -> %G ?\n", x, y[1], tx);
      if (tx > 0) { x= tx; } else { x= min.x; } // + randf(hW)
   } while (++i < iMax);

   printf(": %G %G\n", min.x, min.y);
   return(min.x);
} // searchNewton
