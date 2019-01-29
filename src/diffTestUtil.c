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

   pO->n1F= n;
   n*= pO->nPhase;
   pO->n1B= n;
   return(n > 0);
} // initOrg

void initW (D3S6IsoWeights * pW, DiffScalar r)
{
   const uint f= 1<<8;
   pW->w[1]= r * (f / 6) / f;
   pW->w[0]= 1 - 6 * pW->w[1];
   printf("initW() - w[]=%G,%G\n", pW->w[0], pW->w[1]);
} // initW

static D3S6MapElem getMapElem (Index x, Index y, Index z, const V3I *pMax)
{
   D3S6MapElem m= 0;
   m|= (0 != x) << 0;
   m|= (pMax->x != x) << 1;
   m|= (0 != y) << 2;
   m|= (pMax->y != y) << 3;
   m|= (0 != z) << 4;
   m|= (pMax->z != z) << 5;
   return(m);
} // getMapElem

size_t setDefaultMap (D3S6MapElem *pM, const V3I *pD)
{
   const size_t nP= pD->x * pD->y;
   const size_t nV= nP * pD->z;
   const Stride s[2]= {pD->x,nP};
   const V3I md= { pD->x-1, pD->y-1, pD->z-1 };
   Index x, y, z;
   const D3S6MapElem me= getMapElem(1,1,1,&md);

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
         pM[x + y * s[0] + 0 * s[1]]= getMapElem(x,y,0,&md);
         pM[x + y * s[0] + z * s[1]]= getMapElem(x,y,z,&md);
      }
   }
   for (z=0; z < pD->z; z++)
   {
      y= pD->y-1;
      for (x=0; x < pD->x; x++)
      {
         pM[x + 0 * s[0] + z * s[1]]= getMapElem(x,0,z,&md);
         pM[x + y * s[0] + z * s[1]]= getMapElem(x,y,z,&md);
      }
      x= pD->x-1;
      for (y=0; y < pD->y; y++)
      {
         pM[0 + y * s[0] + z * s[1]]= getMapElem(0,y,z,&md);
         pM[x + y * s[0] + z * s[1]]= getMapElem(x,y,z,&md);
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

DiffScalar compareAnalytic (const DiffScalar * pS, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt)
{
   DiffScalar k[2], sw=0, sad=0;
   V3I i, c;
   c.x= pO->def.x/2; c.y= pO->def.y/2; c.z= pO->def.z/2;

   setDiffIsoK(k, Dt, 3);
   
   for (i.z=0; i.z < pO->def.z; i.z++)
   {
      for (i.y=0; i.y < pO->def.y; i.y++)
      {
         for (i.x=0; i.x < pO->def.x; i.x++)
         {
            float r2= d2F3( i.x - c.x, i.y - c.y, i.z - c.z );
            DiffScalar w= v * k[0] * exp(r2 * k[1]);
            size_t j= i.x * pO->stride[0] + i.y * pO->stride[1] + i.z * pO->stride[2];
            sad+= fabs(pS[j] - w);
            sw+= w;
         }
      }
   }
   if (fabs(sw - v) > gEpsilon) { printf("WARNING: compareAnalytic() - sw=%G\n", sw); }
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

DiffScalar diffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride sr12)
{
   DiffScalar mom[3]= {0,0,0}, sad=0; //, as[2]={0,0};
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
            mom[0]+= 1;
            mom[1]+= d;
            mom[2]+= d * d;
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
            mom[0]+= 1;
            mom[1]+= d;
            mom[2]+= d * d;
         }
      }
   }
   if (mom[0] > 1)
   {
      DiffScalar mean= mom[1] / mom[0];
      DiffScalar var= ( mom[2] - (mom[1] * mean) ) / (mom[0] - 1);
      printf("diffStrideNS() - n,sum,sumSqr,mean,var= %G, %G, %G, %G, %G\n", mom[0], mom[1], mom[2], mean, var);
   }
   return(sad);
} // diffStrideNS

DiffScalar relDiffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride sr12)
{
   DiffScalar mom[3]= {0,0,0};
   for (size_t i=0; i<n; i++)
   {
      //DiffScalar s= pS1[i * sr12] + pS2[i * sr12]; 
      DiffScalar s2= pS2[i * sr12]; 
      DiffScalar rd= 0;
      if (0 != s2)
      {
         rd= (pS1[i * sr12] - s2) / s2;
         mom[0]+= 1;
         mom[1]+= rd;
         mom[2]+= rd * rd;
      }
      pR[i]= rd;
   }
   if (mom[0] > 1)
   {
      DiffScalar mean= mom[1] / mom[0];
      DiffScalar var= ( mom[2] - (mom[1] * mean) ) / (mom[0] - 1);
      printf("relDiffStrideNS() - n,sum,mean,var= %G, %G, %G, %G\n", mom[0], mom[1], mean, var);
   }
   return(mom[1]);
} // diffStrideNS


DiffScalar searchMin1 (const DiffScalar *pS, const DiffOrg *pO, const DiffScalar ma, const DiffScalar Dt0, const DiffScalar Dt1)
{
   SearchPoint p, min[2];
   int r;

   min[0].x= Dt0;
   min[0].y= compareAnalytic(pS, pO, ma, min[0].x);
   min[1].x= Dt1;
   min[1].y= compareAnalytic(pS, pO, ma, min[1].x);
   printf("searchMin1() -\n? %G %G\n? %G %G\n", min[0].x, min[0].y, min[1].x, min[1].y);

   if (min[1].y < min[0].y) { SWAP(SearchPoint, min[0], min[1]); }
   do
   {
      r= 0;
      p.x= 0.5 * (min[0].x + min[1].x);
      p.y= compareAnalytic(pS, pO, ma, p.x);
      printf("? %G %G\n", p.x, p.y);
      if (p.y < min[0].y) { min[1]= min[0]; min[0]= p; r++; }
      else if (p.y < min[1].y) { min[1]= p; r++; }
   } while ((r > 0) && ((min[1].y - min[0].y) > gEpsilon));

   printf(": %G %G\n", min[0].x, min[0].y);
   return(min[0].x);
} // searchMin1

DiffScalar searchNewton (const DiffScalar *pS, const DiffOrg *pO, const DiffScalar ma, const DiffScalar Dt0, const DiffScalar Dt1)
{
   SearchPoint min={0,1E34};
   DiffScalar x, y[3], dy, hW= 0.0625 * (Dt1 - Dt0);
   const int iMax= 10;
   int i= 0, r= 0, m= 0;

   x= Dt0;//0.5 * (Dt1 + Dt0);
   do
   {
      DiffScalar tx= x - hW;
      for (int j= 0; j<3; j++)
      {
         y[j]= compareAnalytic(pS, pO, ma, tx);
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
