// diffTestUtil.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-June 2019

#include "diffTestUtil.h"
//#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { DiffScalar x, y; } SearchPoint;

const float gEpsilon= 1.0 / (1<<30);
const D3MapKeyVal gDefObsKV=
{
   {-1,0}, // mask, value
   0.0, -1.0  // v2[0..1]
};


/***/

void initHack (void)
{
#ifdef NAN
   D3MapKeyVal *pKV= (D3MapKeyVal*)&gDefObsKV; // HACK! const violation
   pKV->v2[1]= NAN;
   LOG("initHack() - NAN set: %G\n", pKV->v2[1]);
#endif
} // initHack

size_t initFieldVCM (DiffScalar * pS, const DiffOrg * pO, const D3MapElem * pM, const D3MapKeyVal *pKV, const MapSiteInfo * pMSI)
{
   size_t r=0;
   if (pM)
   {
      const Stride mapStride[3]={1, pO->def.x, pO->def.x * pO->def.z};
      if (NULL == pKV) { pKV= &gDefObsKV; }
      for (Index z= 0; z < pO->def.z; z++)
      {
         for (Index y= 0; y < pO->def.y; y++)
         {
            for (Index x= 0; x < pO->def.z; x++)
            {
               const size_t iM= dotS3(x,y,z, mapStride); // x * mapStride[0] + y * mapStride[1] + z * mapStride[2];
               const size_t iS= dotS3(x,y,z, pO->stride); //x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
               int t= ( pKV->k.v == (pKV->k.m & pM[iM]) );
               pS[iS]= pKV->v2[ t ];
               r+= t;
            }
         }
      }
   }
   else
   {
      memset(pS, 0, pO->n1B * sizeof(*pS));
      //r= pO->n1B;
   }
   if (pMSI)
   {
      const size_t i= pMSI->c.x * pO->stride[0] + pMSI->c.y * pO->stride[1] + pMSI->c.z * pO->stride[2];
      pS[i]= pMSI->v;
   }
   return(r);
} // initFieldVCM

size_t resetFieldVCM (DiffScalar * pS, const DiffOrg *pO, const D3MapElem *pM, const D3MapKey *pK, DiffScalar v)
{
   size_t r=0;
   if (pM)
   {
      const Stride mapStride[3]={1, pO->def.x, pO->def.x * pO->def.z};

      if (NULL == pK) { pK= &(gDefObsKV.k); }
      for (Index z= 0; z < pO->def.z; z++)
      {
         for (Index y= 0; y < pO->def.y; y++)
         {
            for (Index x= 0; x < pO->def.z; x++)
            {
               const size_t iM= dotS3(x,y,z,mapStride);//x * mapStride[0] + y * mapStride[1] + z * mapStride[2];
               if (pK->v == (pK->m & pM[iM]))
               { 
                  const size_t iS= dotS3(x,y,z,pO->stride);// x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
                  pS[iS]= v;
                  r++;
               }
            }
         }
      }
   }
   return(r);
} // resetFieldVCM

float d2F3 (float dx, float dy, float dz) { return( dx*dx + dy*dy + dz*dz ); }

float setDiffIsoK (DiffScalar k[2], const DiffScalar Dt, const U32 dim)
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
               const size_t j= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];//dotS3(x,y,z,pO->stride);//
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

            size_t j= dotS3(i.x,i.y,i.z,pO->stride);//i.x * pO->stride[0] + i.y * pO->stride[1] + i.z * pO->stride[2];
            t+= pR[j]= v * k[0] * exp(r2 * k[1]);
            //t[(r2 > r2m)]+= pR[j];
         }
      }
   }
   //for (size_t j=0; j < pO->n1F; j++) { t[2]+= pR[j]; }
   //printf("initPhaseAnalytic() - rm (4s) =%G, t=%G,%G,%G (%G)\n", rm, t[0], t[1], t[2], t[0]+t[1]);
   return(t);
} // initAnalytic

DiffScalar analyseField (AnResD3R2 * pR, const DiffScalar * pS, const DiffOrg *pO)
{
   StatMomD3R2 sm3={0,};
   MMSMVal mm;

   mm.vMin= mm.vMax= pS[0];
   for (Index z=0; z < pO->def.z; z++)
   {
      for (Index y=0; y < pO->def.y; y++)
      {
         for (Index x=0; x < pO->def.x; x++)
         {
            const size_t i= dotS3(x,y,z,pO->stride);//x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
            statMom3AddW(&sm3, x, y, z, pS[i]);
            mm.vMin= fmin(mm.vMin, pS[i]);
            mm.vMax= fmax(mm.vMax, pS[i]);
         }
      }
   }
   if (pR)
   {
      StatResD1R2 r[3];
      statMom3Res1(r, &sm3, 0);
      for (int i=0; i<3; i++)
      {
         pR->mean[i]= r[i].m;
         pR->var[i]= r[i].v;
      }
      pR->sum= sm3.m0;
      pR->mm= mm;
   }
   return(sm3.m0);
} // analyseField

void minMaxNSMV (MMSMVal *pMM, const SMVal v[], const size_t n, const Stride s)
{
   MMSMVal mm;
   mm.vMin= mm.vMax= v[0];
   for (size_t i=1; i < n; i++)
   {
      size_t j= i * s;
      mm.vMin= fmin(mm.vMin, v[j]);
      mm.vMax= fmax(mm.vMax, v[j]);
   }
   *pMM= mm;
} // minMaxNSMV

size_t saveSliceRGB (const char path[], const DiffScalar * pS, const DiffOrg *pO, const MMSMVal *pMM)
{
   unsigned char *pB;
   size_t nB= pO->n1P * 3;
   pB= malloc(nB);
   if (pB)
   {
      MMSMVal mm, rmm={1,1};

      if (NULL == pMM)
      {
         pMM= &mm;
         minMaxNSMV(&mm, pS, pO->n1P, pO->stride[0]);
         printf("saveSliceRGB() - mm=%G,%G\n",mm.vMin,mm.vMax);
      }
      if (0 != pMM->vMin) { rmm.vMin= 1.0 / pMM->vMin; }
      if (0 != pMM->vMax) { rmm.vMax= 1.0 / pMM->vMax; }
      for (size_t i=0; i < pO->n1P; i++)
      {  // HACK! assuming 1D equivalence
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
   return(0);
} // saveSliceRGB

DiffScalar searchMin1 
(
   SearchResult *pR, const MemBuff * pWS,
   const DiffScalar *pS, const DiffOrg *pO,
   const DiffScalar ma, const DiffScalar Dt, const U32 f
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
   SMVal sum, uMin, vMin, vMax;

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
   uMin= -1;
   if ((vMax > 0) && (uMin <= 0))
   {
      uMin= vMax;
      for (size_t i=1; i < n; i++) { if (pS[i] > 0) { uMin= fmin(uMin, pS[i]); } }
   }
   pR->sum= sum;
   pR->uMin= uMin;
   pR->mm.vMin= vMin;
   pR->mm.vMax= vMax;
} // reduct1F0

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

void reduct3_2_0 (RedRes * pR, DiffScalar * restrict pTR, const DiffScalar * const pS, const DiffOrg *pO, const char map)
{
   #pragma acc data present_or_create( pTR[:pO->n1P] ) present_or_copyin( pS[:pO->n1F], pO[:1] ) copyout( pTR[:pO->n1P] )
   {
      reduct3P2(pTR, pS, pO);
      reduct1F0(pR, pTR, pO->n1P);
      if ('L' == map)
      {
         #pragma acc parallel
         for (size_t i=1; i < pO->n1P; i++) { pTR[i]= log( pTR[i] ); }
      }
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

void dumpM6 (U32 m6, const char *e)
{
   char a[]="XYZ";
   char s[]="-+";
   for (int i= 0; i < 6; i++)
   {
      if (m6 & (1<<i)) { printf("%c%c ", s[i&1], a[i>>1] ); } else { printf("   "); }
   }
   if (e) { printf("%s", e); }
} // dumpM6

void dumpM8 (U32 m8, const char *e)
{
   char a[]="XYZ";
   char s[]="-+";
   for (int i= 0; i < 8; i++)
   {
      //if (m8 & (1<<i)) { printf("%c%c ", s[i&1], a[i>>1] ); } else { printf("   "); }
   }
   if (e) { printf("%s", e); }
} // dumpM6

void dumpDistBC (const D3MapElem * pM, const size_t nM)
{
   size_t d1[27], d2[7], s= 0;
   for (int i=0; i<=26; i++) { d1[i]= 0; }
   for (int i=0; i<=6; i++) { d2[i]= 0; }
   for (size_t i=0; i<nM; i++)
   {
      U32 b= bitCountZ( pM[i] & ((1<<26)-1) );
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

void dumpDMMBC (const U8 *pU8, const D3MapElem * pM, const size_t n, const U32 mask)
{
   size_t d[33], t= 0;
   MMU8 mm[33];
   for (int i=0; i<=32; i++) { mm[i].vMin= 0xFF; mm[i].vMax= 0; d[i]= 0; }
   for (size_t i=0; i < n; i++)
   {
      U32 b= bitCountZ( pM[i] & mask );
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
            U32 m6= m & ((1<<6)-1);
            U32 m12= (m>>6) & ((1<<12)-1);
            U32 m8= (m>>18) & ((1<<8)-1);
            printf("(%3d,%3d,%3d) : %u ", v[i].x, v[j].y, v[k].z, r);
            printf("m: 0x%x(%u) 0x%x(%u) 0x%x(%u) ", m6, bitCountZ(m6), m12, bitCountZ(m12), m8, bitCountZ(m8));
            dumpM6(m,"\n"); 
         }
      }
   }
} // checkComb
