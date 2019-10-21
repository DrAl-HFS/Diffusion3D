// diffTestHacks.c - Dumping ground for hacky test code.
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-Oct 2019

#include "diffTestHacks.h"


typedef struct
{
   const char *fs, *eol;
} FmtDesc;

/***/

int dumpScalarRegion (char *pCh, int maxCh, const DiffScalar * pS, const MMV3I *pRegion, const Stride s[3])
{
   FmtDesc fd={"%.3G ","\n"};
   int nCh= 0;
   for (Index z= pRegion->vMin.z; z <= pRegion->vMax.z; z++)
   {
      for (Index y= pRegion->vMin.y; y <= pRegion->vMax.y; y++)
      {
         for (Index x= pRegion->vMin.x; x <= pRegion->vMax.x; x++)
         {
            const size_t i= x * s[0] + y * s[1] + z * s[2];
            nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.fs, pS[i]);
         }
         nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol);
      }
      nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol);
   }
   return(nCh);
} // dumpScalarRegion

int dumpMapRegion (char *pCh, int maxCh, const D3MapElem * pM, const MMV3I *pRegion, const Stride s[3])
{
   FmtDesc fd={"0x%x ","\n"};
   int nCh= 0;
   for (Index z= pRegion->vMin.z; z <= pRegion->vMax.z; z++)
   {
      for (Index y= pRegion->vMin.y; y <= pRegion->vMax.y; y++)
      {
         for (Index x= pRegion->vMin.x; x <= pRegion->vMax.x; x++)
         {
            const size_t i= x * s[0] + y * s[1] + z * s[2];
            nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.fs, pM[i]);
         }
         nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol);
      }
      nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol);
   }
   return(nCh);
} // dumpMapRegion

void dumpSMR (const DiffScalar * pS, const D3MapElem * pM, const V3I *pC, int a, const DiffOrg *pO)
{
   const MMV3I mm= {pC->x-a,pC->y-a,pC->z-a, pC->x+a,pC->y+a,pC->z+a};
   char buff[1<<14];
   const int m= sizeof(buff)-1;
   //adjustMMV3I(&mm, a);
   int n= dumpScalarRegion(buff, m, pS, &mm, pO->stride);

   if (pM)
   {
      Stride s[3]={ 1, pO->def.x, pO->def.x * pO->def.y };

      n+= dumpMapRegion(buff+n, m-n, pM, &mm, s);
   }
   if (n > 0) { printf("C(%d,%d,%d):-\n%s", pC->x, pC->y, pC->z, buff); }
} // dumpSMR

float *allocNF (FBuf *pFB, const int n)
{
   float *pF= NULL;
   if ((pFB->nF+n) < pFB->maxF)
   { 
      pF= pFB->pF + pFB->nF;
      pFB->nF+= n;
   } 
   return(pF);
} // allocNF

void addFB (FBuf *pFB, const float v)
{
   if (pFB->nF < pFB->maxF) { pFB->pF[pFB->nF++]= v; }
   //LOG("FB%d/%d ",pFB->nF,pFB->maxF);
} // addFB
