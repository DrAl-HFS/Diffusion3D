// diffTestUtil.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diffTestUtil.h"

typedef struct { DiffScalar x, y; } SearchPoint;

const float gEpsilon= 1.0 / (1<<30);


/***/

void initFieldVC (DiffScalar * pS, const DiffOrg *pO, DiffScalar v, const V3I *pC)
{
   memset(pS, 0, pO->n1B * sizeof(*pS));

   const size_t i= pC->x * pO->stride[0] + pC->y * pO->stride[1] + pC->z * pO->stride[2];
   pS[i]= v;
} // initFieldVC

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
      #pragma acc loop seq
      for (Index z=0; z < pO->def.z; z++)
      {
         #pragma acc parallel loop 
         for (Index y=0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x=0; x < pO->def.x; x++)
            {
               const float r2= d2F3( x - c.x, y - c.y, z - c.z );
               const DiffScalar w= v * k[0] * exp(r2 * k[1]);
               const size_t j= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
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
   #pragma acc data present_or_create( pTR[:pO->n1P] ) copyin( pS[:pO->n1F], pO[:1] ) copyout( pTR[:pO->n1P] )
   {
      sad= compareAnalyticP(pTR,pS,pO,v,Dt);
   }
   return(sad);
} // compareAnalytic

DiffScalar initAnalytic (DiffScalar * pR, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt)
{
   DiffScalar k[2], t= 0; //sigma, rm, r2m, t[3]= {0,0,0};
   V3I i, c;
   c.x= pO->def.x/2; c.y= pO->def.y/2; c.z= pO->def.z/2;

   //sigma= 
   setDiffIsoK(k, Dt, 3);
   //rm= 4 * sigma;
   //r2m= rm * rm;

   for (i.z=0; i.z < pO->def.z; i.z++)
   {
      for (i.y=0; i.y < pO->def.y; i.y++)
      {
         for (i.x=0; i.x < pO->def.x; i.x++)
         {
            float r2= d2F3( i.x - c.x, i.y - c.y, i.z - c.z );

            size_t j= i.x * pO->stride[0] + i.y * pO->stride[1] + i.z * pO->stride[2];
            t+= pR[j]= v * k[0] * exp(r2 * k[1]);
            //t[(r2 > r2m)]+= pR[j];
         }
      }
   }
   //for (size_t j=0; j < pO->n1F; j++) { t[2]+= pR[j]; }
   //printf("initPhaseAnalytic() - rm (4s) =%G, t=%G,%G,%G (%G)\n", rm, t[0], t[1], t[2], t[0]+t[1]);
   return(t);
} // initAnalytic

DiffScalar analyseField (StatRes1 r[3], const DiffScalar * pS, const DiffOrg *pO)
{
   StatMom3 sm3={0,};
   DiffScalar tv=0;

   for (Index z=0; z < pO->def.z; z++)
   {
      for (Index y=0; y < pO->def.y; y++)
      {
         for (Index x=0; x < pO->def.x; x++)
         {
            const size_t i= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
            statMom3AddW(&sm3, x, y, z, pS[i]);
         }
      }
   }
   statMom3Res1(r, &sm3, 0);
   return((r[0].v + r[1].v + r[2].v) / 3);
} // analyseField

size_t saveSliceRGB (const char path[], const DiffScalar * pS, const uint z, const DiffOrg *pO, const MMSMVal *pMM)
{
   if (z < pO->def.z)
   {
      unsigned char *pB;
      size_t nB= pO->n1P * 3;
      pB= malloc(nB);
      if (pB)
      {
         MMSMVal mm, rmm={1,1};

         if (NULL == pMM)
         {
            mm.vMin= mm.vMax= pS[0];
            for (size_t i=1; i < pO->n1P; i++)
            {
               size_t j= i * pO->stride[0];
               mm.vMin= fmin(mm.vMin, pS[j]);
               mm.vMax= fmax(mm.vMax, pS[j]);
            }
            printf("saveSliceRGB() - mm=%G,%G\n",mm.vMin,mm.vMax);
            pMM= &mm;
         }
         if (0 != pMM->vMin) { rmm.vMin= 1.0 / pMM->vMin; }
         if (0 != pMM->vMax) { rmm.vMax= 1.0 / pMM->vMax; }
         for (size_t i=0; i < pO->n1P; i++)
         {
            size_t j= i * pO->stride[0];
            size_t k= i * 3;
            unsigned char r= 0, g=0, b= 0;
            if (pS[j] > 0) //gEpsilon)
            {
               float t= rmm.vMax * pS[j];
               g= 0x00 + t * 0xF0;
               b= 0x40 + t * 0xBF;
            }
            else if (pS[j] < 0)
            {
               float t= rmm.vMin * pS[j];
               g= 0x00 + t * 0xF0;
               r= 0x40 + t * 0xBF;
            }

            pB[k+0]= r;
            pB[k+1]= g;
            pB[k+2]= b;
         }
         //pB[0]= pB[1]= pB[2]= 0xFF;
         nB= saveBuff(pB, path, nB);
         printf("saveSliceRGB(%s) - %zu bytes\n", path, nB);
         free(pB);
         return(nB);
      }
   }
   return(0);
} // saveSliceRGB

DiffScalar searchMin1 
(
   SearchResult *pR, const MemBuff * pWS,
   const DiffScalar *pS, const DiffOrg *pO,
   const DiffScalar ma, const DiffScalar Dt, const uint f
)
{
   DiffScalar  *pTR= pWS->p;
   SearchPoint p, min[2];
   int r=2;

   #pragma acc data create( pTR[:pO->n1P] ) copyin( pS[:pO->n1F], pO[:1] ) copyout( pTR[:pO->n1P] )
   {
      float w= ((f >> 1) & 0xFF) / 0xFF;
      //p.y=  compareAnalytic(pS, pO, ma, Dt);
      min[0].x= (1-w) * Dt;
      min[0].y= compareAnalyticP(pTR, pS, pO, ma, min[0].x);
      if (w > 0)
      {
         min[1].x= (1+w) * Dt;
         min[1].y= compareAnalyticP(pTR, pS, pO, ma, min[1].x);
         if (min[1].y < min[0].y) { SWAP(SearchPoint, min[0], min[1]); }
         if (f & 1) { printf("searchMin1() -\n  Dt   SAD\n? %G %G\n? %G %G\n", min[0].x, min[0].y, min[1].x, min[1].y); }

         do
         {
            p.x= 0.5 * (min[0].x + min[1].x);
            p.y= compareAnalyticP(pTR, pS, pO, ma, p.x);
            if (f & 1) { printf("? %G %G\n", p.x, p.y); }
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
   }
   if (pR)
   {
      pR->Dt=  min[0].x;
      pR->sad= min[0].y;
   }
   if (f & 1) { printf(": %G %G\n", min[0].x, min[0].y); }
   return(min[0].x);
} // searchMin1

// Full 1D -> 0D reduction
static void reduct1F0 (RedRes * pR, const DiffScalar * const pS, const size_t n)
{
   SMVal sum, vMin, vMax; // =0, vMin=1E34, vMax=-1E34;

   sum= vMin= vMax= *pS;
   #pragma acc data present( pS[:n] ) copy( sum, vMin, vMax ) // n ???
   {
      #pragma acc parallel reduction(+: sum )
      for (size_t i=1; i < n; i++) { sum+= pS[i]; }
      #pragma acc parallel reduction(min: vMin )
      for (size_t i=1; i < n; i++) { vMin= fmin(vMin, pS[i]); }
      #pragma acc parallel reduction(max: vMax )
      for (size_t i=1; i < n; i++) { vMax= fmax(vMax, pS[i]); }
   }
   pR->sum= sum;
   pR->mm.vMin= vMin;
   pR->mm.vMax= vMax;
} // reduct0F1

// Partial 3D -> 2D reduction
static void reduct3P2 (DiffScalar * restrict pTR, const DiffScalar * const pS, const DiffOrg *pO)
{
   #pragma acc data present( pTR[:pO->n1P], pO[:1] )
   {
      #pragma acc parallel loop vector
      for (Index i=0; i < pO->n1P; i++) { pTR[i]= 0; }
   }

   #pragma acc data present( pTR[:pO->n1P], pS[:pO->n1F], pO[:1] )
   {
      #pragma acc loop seq
      for (Index z=0; z < pO->def.z; z++)
      {
         #pragma acc parallel loop 
         for (Index y=0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x=0; x < pO->def.x; x++)
            {
               const size_t j= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
               pTR[x + y * pO->def.x]+= pS[j];
            }
         }
      }
   }
} // reduct3P2

void reduct0 (RedRes * pR, const DiffScalar * const pS, const size_t n)
{
   #pragma acc data present_or_copyin( pS[:n] )
   reduct1F0(pR,pS,n);
} // reduct0

void reduct3_2_0 (RedRes * pR, DiffScalar * restrict pTR, const DiffScalar * const pS, const DiffOrg *pO)
{
   #pragma acc data present_or_create( pTR[:pO->n1P] ) present_or_copyin( pS[:pO->n1F], pO[:1] ) copyout( pTR[:pO->n1P] )
   {
      reduct3P2(pTR, pS, pO);
      reduct1F0(pR, pTR, pO->n1P);
   }
} // reduct3_2_0

// DEPRECATE
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
/*
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
   StatMom1 mom={0,0,0};
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
   
   if (statMom1Res1(&sr, &mom, 1))
   {
      printf("diffStrideNS() - n,sum,sumSqr,mean,var= %G, %G, %G, %G, %G\n", mom.m[0], mom.m[1], mom.m[2], sr.m, sr.v);
   }
   return(sad);
} // diffStrideNS

SMVal relDiffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride sr12)
{
   StatMom1 mom={0,0,0};
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
   if (statMom1Res1(&sr, &mom, 1))
   {
      printf("diffStrideNS() - n,sum,sumSqr,mean,var= %G, %G, %G, %G, %G\n", mom.m[0], mom.m[1], mom.m[2], sr.m, sr.v);
   }
   return(sad);
} // relDiffStrideNS
*/

void dumpM6 (uint m6, const char *e)
{
   char a[]="XYZ";
   char s[]="-+";
   for (int i= 0; i < 6; i++)
   {
      if (m6 & (1<<i)) { printf("%c%c ", s[i&1], a[i>>1] ); } else { printf("   "); }
   }
   if (e) { printf("%s", e); }
} // dumpM6

void dumpM8 (uint m8, const char *e)
{
   char a[]="XYZ";
   char s[]="-+";
   for (int i= 0; i < 8; i++)
   {
      //if (m8 & (1<<i)) { printf("%c%c ", s[i&1], a[i>>1] ); } else { printf("   "); }
   }
   if (e) { printf("%s", e); }
} // dumpM6
