// diffTestUtil.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diffTestUtil.h"

/***/


bool initOrg (DiffOrg *pO, uint def, uint nP)
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

void setDiffIsoK (DiffScalar k[2], const DiffScalar Dt, const uint dim)
{
   const DiffScalar msd= 2 * Dt; // mean squared distance = variance
   k[0]= pow(2 * M_PI * msd, -0.5 * dim);
   k[1]= -0.5 / msd;
} // setDiffIsoK

DiffScalar initPhaseAnalytic (DiffScalar * pS, const DiffOrg *pO, const uint phase, const DiffScalar v, const DiffScalar Dt)
{
   if (phase < pO->nPhase)
   {
      DiffScalar k[2], t= 0;
      V3F c;
      V3I i;
      c.x= pO->def.x/2; c.y= pO->def.y/2; c.z= pO->def.z/2;

      setDiffIsoK(k, Dt, 3);
      pS+= phase * pO->phaseStride;
      for (i.z=0; i.z < pO->def.z; i.z++)
      {
         for (i.y=0; i.y < pO->def.y; i.y++)
         {
            for (i.x=0; i.x < pO->def.x; i.x++)
            {
               float r2= d2F3( i.x - c.x, i.y - c.y, i.z - c.z );

               size_t j= i.x * pO->stride[0] + i.y * pO->stride[1] + i.z * pO->stride[2];
               pS[j]= v * k[0] * exp(r2 * k[1]);
            }
         }
      }
      for (size_t j=0; j < pO->n1F; j++) { t+= pS[j]; }
      return(t);
   }
   return(0);
} // initPhaseAnalytic

size_t saveBuff (const void * const pB, const char * const path, const size_t bytes)
{
   FILE *hF= fopen(path,"w");
   if (hF)
   {
      size_t r= fwrite(pB, 1, bytes, hF);
      fclose(hF);
      if (r == bytes) { return(r); }
   }
   return(0);
} // saveBuff

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
         const DiffScalar eps= 1.0 / (1<<30);
         DiffScalar rm=0, m= -1;
         pS+= phase * pO->phaseStride + z * pO->stride[2];
         for (size_t i=0; i < n1P; i++)
         {
            size_t j= i * pO->stride[0];
            m= MAX(m, pS[j]);
         }
         if (m > 0) { rm= 1.0 / m; }
         for (size_t i=0; i < n1P; i++)
         {
            size_t j= i * pO->stride[0];
            size_t k= i * 3;
            unsigned char r= 0, g=0, b= 0;
            if (pS[j] > eps)
            {
               float t= rm * pS[j];
               r= g= 0x00 + t * 0xF0;
               b= 0x40 + t * 0xBF;
            }
            pB[k+0]= r;
            pB[k+1]= g;
            pB[k+2]= b;
         }
         nB= saveBuff(pB, path, nB);
         printf("saveSliceRGB(%s ) - (m=%G) %zu bytes\n", path, m, nB);
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
   DiffScalar mom[3]= {0,0,0};
   if (pR)
   {
      for (size_t i=0; i<n; i++)
      {
         DiffScalar s= pS1[i * sr12] + pS2[i * sr12]; 
         DiffScalar d= pS1[i * sr12] - pS2[i * sr12]; 
         pR[i]= d;
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
      printf("diffStrideNS() - n,sum,mean,var= %G, %G, %G, %G\n", mom[0], mom[1], mean, var);
   }
   return(mom[1]);
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

