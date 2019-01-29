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
   MemBuff        ws;
} DiffTestContext;


static DiffTestContext gCtx={0,};

/***/




void analyse (const DiffScalar * pS1, const DiffScalar * pS2, const int phase, const DiffOrg *pO)
{
   // HACKY! may break for multi-phase
   //DiffScalar d= relDiffStrideNS(gCtx.ws.p, pS1, pS2, gCtx.org.n1F, gCtx.org.stride[0]);
   DiffScalar d= diffStrideNS(gCtx.ws.p, pS1, pS2, gCtx.org.n1F, gCtx.org.stride[0]);
   /*
   DiffScalar s1= sumField(pS1, phase, pO);
   DiffScalar s2= sumField(pS2, phase, pO);
   printf("sums= %G, %G\n", s1, s2);
   */
   saveSliceRGB("rgb/diff.rgb", gCtx.ws.p, 0, pO->def.z / 2, &(gCtx.org));
} // analyse


Bool32 init (DiffTestContext *pC, uint def)
{
   if (initOrg(&(pC->org), def, 1))
   {
      size_t b1M= pC->org.n1F * sizeof(*(pC->pM));
      size_t b1B= pC->org.n1B * sizeof(*(pC->pSR));
      uint nB= 0;

      initW(pC->w, 0.5);
      pC->pM= malloc(b1M);
      if (pC->pM)
      {
         printf("pM: %p, %zuB, bndsum=%zu\n", pC->pM, b1M, setDefaultMap(pC->pM, &(pC->org.def)) );
         nB++;
      }
      pC->ws.bytes=  1<<28;
      pC->ws.p=  malloc(pC->ws.bytes);
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
   if (pC->ws.p) { free(pC->ws.p); pC->ws.p= NULL; }
   for (int i= 0; i<2; i++) { if (pC->pSR[i]) { free(pC->pSR[i]); pC->pSR[i]= NULL; } }
} // release


int main (int argc, char *argv[])
{
   uint iT=0, iN= 0, iA= 0;
   int r= 0;
   if (init(&gCtx,1<<8))
   {
      const DiffScalar m= 1.0;
      const Index zSlice= gCtx.org.def.z / 2;

      defFields(gCtx.pSR[0], &(gCtx.org), m);

      //pragma acc set device_type(acc_device_none)
      iT+= diffProcIsoD3S6M(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.w, gCtx.pM, 100);
      iN= iT & 1;
      saveSliceRGB("rgb/numerical.rgb", gCtx.pSR[iN], 0, zSlice, &(gCtx.org));
      iA= iN^1;

      // Search for Diffusion-time moment
      DiffScalar Dt= searchMin1(gCtx.pSR[iN], &(gCtx.org), m, 7.95, 8.25); // 8.18516
      //Newtons method performs relatively poorly - due to discontinuity?
      //DiffScalar Dt= searchNewton(gCtx.pSR[iN], &(gCtx.org), m, 7.95, 8.25);

      initPhaseAnalytic(gCtx.pSR[iA], &(gCtx.org), 0, m, Dt);
      saveSliceRGB("rgb/analytic.rgb", gCtx.pSR[iA], 0, zSlice, &(gCtx.org));

      analyse(gCtx.pSR[iA], gCtx.pSR[iN], 0, &(gCtx.org));
   }
   release(&gCtx);
   printf("Complete\n");
   return(r);
} // main
