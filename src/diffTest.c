// diffTest.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diffTestUtil.h"

typedef struct
{
   DiffOrg        org;
   DiffScalar     *pSR[2];
   D3IsoWeights   wPhase[1];
   D3MapElem      *pM;
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
      size_t b1W= pC->org.n1F * sizeof(*(pC->pSR));
      uint nB= 0;

      pC->pM= malloc(b1M);
      if (pC->pM)
      {
         printf("pM: %p, %zuB, bndsum=%zu\n", pC->pM, b1M, setDefaultMap(pC->pM, &(pC->org.def)) );
         nB++;
      }
      pC->ws.bytes=  b1W; // 1<<28; ???
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

typedef struct
{
   const char *fs, *eol[2];
} FmtDesc;

int dumpRegion (char *pCh, int maxCh, const DiffScalar * pS, const MMV3I *pRegion, const DiffOrg *pO)
{
   FmtDesc fd={"%G ","\n","\n"};
   int nCh= 0;
   for (Index z= pRegion->min.z; z <= pRegion->max.z; z++)
   {
      for (Index y= pRegion->min.y; y <= pRegion->max.y; y++)
      {
         for (Index x= pRegion->min.x; x <= pRegion->max.x; x++)
         {
            const size_t i= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
            nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.fs, pS[i]);
         }
         nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol[0]);
      }
      nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol[1]);
   }
   return(nCh);
} // dumpRegion

void dump (const DiffScalar * pS)
{
   const V3I c= {gCtx.org.def.x/2, gCtx.org.def.y/2, gCtx.org.def.z/2};
   const MMV3I mm= {c.x-1,c.y-1,c.z-1, c.x+1,c.y+1,c.z+1};
   char buff[1<<12];
   if (dumpRegion(buff, sizeof(buff)-1, pS, &mm, &(gCtx.org)))
   {
      printf("%s", buff);
   }
} // dump

int main (int argc, char *argv[])
{
   SMVal dT;
   uint iT=0, iN= 0, iA= 0, nHood=26;
   int r= 0;
   if (init(&gCtx,1<<8))
   {
      const DiffScalar m= 1.0;
      const Index zSlice= gCtx.org.def.z / 2;

      defFields(gCtx.pSR[0], &(gCtx.org), m);

      //test(&(gCtx.org));

      deltaT();
      //pragma acc set device_type(acc_device_none)

      //initW(gCtx.wPhase[0].w, 0.5, 6, 0); // ***M8***
      //iT= diffProcIsoD3S6M(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), (D3S6IsoWeights*)(gCtx.wPhase), gCtx.pM, 100);

      nHood=26;
      if (initW(gCtx.wPhase[0].w, 0.5, nHood, 0) > 0)
      {
         iT= diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, 100, nHood);
         dT= deltaT();
         iN= iT & 1;
         DiffScalar s= sumField(gCtx.pSR[iN], 0, &(gCtx.org));
         printf("diffProc..%u %u iter %G sec (%G msec/iter) sum=%G\n", nHood, iT, dT, 1000*dT / iT, s);

         //dump(gCtx.pSR[iN]);

         saveSliceRGB("rgb/numerical.rgb", gCtx.pSR[iN], 0, zSlice, &(gCtx.org));
         iA= iN^1;
#if 1
         // Search for Diffusion-time moment
         DiffScalar Dt= sqrt(iT);
         Dt= searchMin1(&(gCtx.ws), gCtx.pSR[iN], &(gCtx.org), m, Dt); // 8.18516
         //Newtons method performs relatively poorly - due to discontinuity?
         //Dt= searchNewton(&(gCtx.ws), gCtx.pSR[iN], &(gCtx.org), m, Dt);
         saveSliceRGB("rgb/sad.rgb", gCtx.ws.p, 0, 0, &(gCtx.org));

         initPhaseAnalytic(gCtx.pSR[iA], &(gCtx.org), 0, m, Dt);
         saveSliceRGB("rgb/analytic.rgb", gCtx.pSR[iA], 0, zSlice, &(gCtx.org));
         analyse(gCtx.pSR[iA], gCtx.pSR[iN], 0, &(gCtx.org));
#endif
      }
   }
   release(&gCtx);
   printf("Complete\n");
   return(r);
} // main
