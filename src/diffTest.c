// diffTest.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diffTestUtil.h"

typedef struct
{
   DiffOrg        org;
   DiffScalar     *pSR[2];
   D3S6IsoWeights w[1];
   D3S6MapElem    *pM;
} DiffTestContext;


static DiffTestContext gCtx={0,};

/***/

void analyse (const DiffScalar * pS1, const DiffScalar * pS2, const int phase, const DiffOrg *pO)
{
   DiffScalar d= diffStrideNS(NULL, pS1, pS2, gCtx.org.n1F, gCtx.org.stride[0]);
   DiffScalar s1= sumField(pS1, phase, pO);
   DiffScalar s2= sumField(pS2, phase, pO);
   printf("diff= %G sums= %G, %G\n", d, s1, s2);
} // analyse


bool init (DiffTestContext *pC, uint def)
{
   if (initOrg(&(pC->org), def, 1))
   {
      size_t b1M= pC->org.n1F * sizeof(*(pC->pM));
      size_t b1B= pC->org.n1B * sizeof(*(pC->pSR));
      uint nB= 0;

      initW(pC->w, 1);
      pC->pM= malloc(b1M);
      if (pC->pM)
      {
         printf("pM: %p, %zuB, bndsum=%zu\n", pC->pM, b1M, setDefaultMap(pC->pM, &(pC->org.def)) );
         nB++;
      }
      if (nB > 0)
      {
         for (int i= 0; i<2; i++)
            { pC->pSR[i]= malloc(b1B); nB+= (NULL != pC->pSR[i]); }
      }
      return(3 == nB);
   }
   return(0);
} // init

void release (DiffTestContext *pC)
{
   if (pC->pM) { free(pC->pM); pC->pM= NULL; }
   for (int i= 0; i<2; i++) { if (pC->pSR[i]) { free(pC->pSR[i]); pC->pSR[i]= NULL; } }
} // release


int main (int argc, char *argv[])
{
   uint iT=0, iF= 0;
   int r= 0;
   if (init(&gCtx,1<<8))
   {
      const Index zSlice= gCtx.org.def.z / 2;
      defFields(gCtx.pSR[0], &(gCtx.org), 1); // 1, 16.0625
      printf("initPhaseAnalytic() %G\n", initPhaseAnalytic(gCtx.pSR[1], &(gCtx.org), 0, 0.125, 4.03125));
      saveSliceRGB("rgb/analytic.rgb", gCtx.pSR[1], 0, zSlice, &(gCtx.org));

      //pragma acc set device_type(acc_device_none)
      iT+= diffProcIsoD3S6M(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.w, gCtx.pM, 100);
      iF= iT & 1;
      saveSliceRGB("rgb/numerical.rgb", gCtx.pSR[iF], 0, zSlice, &(gCtx.org));

      analyse(gCtx.pSR[iF], gCtx.pSR[iF^1], 0, &(gCtx.org));
   }
   release(&gCtx);
   printf("Complete\n");
   return(r);
} // diffProcIsoD3S6M
