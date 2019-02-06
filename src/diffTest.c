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

const DiffScalar gM= 1.0;


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
   saveSliceRGB("rgb/diff.rgb", gCtx.ws.p, 0, pO->def.z / 2, &(gCtx.org), NULL);
} // analyse

Bool32 init (DiffTestContext *pC, uint def)
{
   if (initDiffOrg(&(pC->org), def, 1))
   {
      size_t b1M= pC->org.n1F * sizeof(*(pC->pM));
      size_t b1B= pC->org.n1B * sizeof(*(pC->pSR));
      size_t b1W= pC->org.n1F * sizeof(*(pC->pSR));
      uint nB= 0;

      pC->pM= malloc(b1M);
      pC->ws.bytes=  b1W; // 1<<28; ???
      pC->ws.p=  malloc(pC->ws.bytes);
      for (int i= 0; i<2; i++)
         { pC->pSR[i]= malloc(b1B); nB+= (NULL != pC->pSR[i]); }
      return(2 == nB);
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

typedef struct
{
   uint nHood, iter;
   DiffScalar  rD;
} Test1Param;

typedef struct
{
   SMVal          tProc;
   SearchResult  sr;
} Test1Res;

uint test1 (Test1Res *pR, const Test1Param *pP, const MapInfo *pMI)
{
   uint iT=0, iN= 0, iA= 0;

   if (initIsoW(gCtx.wPhase[0].w, pP->rD, pP->nHood, 0) > 0)
   {
      defFields(gCtx.pSR[0], &(gCtx.org), gM, &(pMI->m));
      deltaT();
      iT= diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, pP->iter, pP->nHood);
      pR->tProc= deltaT();
      iN= iT & 1;
      // Search for Diffusion-time moment
      StatRes1 r[3];
      DiffScalar isovar= analyseField(r, gCtx.pSR[iN], &(gCtx.org));
      //printf("analyseField() - iso=%G, m=%G,%G,%G, v=%G,%G,%G\n", isovar, r[0].m, r[1].m, r[2].m, r[0].v, r[1].v, r[2].v);
      DiffScalar Dt= sqrt(iT * 2 * pP->rD);
      Dt= searchMin1(&(pR->sr), &(gCtx.ws), gCtx.pSR[iN], &(gCtx.org), gM, 0.5 * isovar, 0);
   }
   return(iT);
} // test1

void redCheck (MMSMVal *pMM, const DiffScalar *pN, const DiffScalar *pA, uint f)
{
   RedRes rN, rA;
   SMVal dt;

   deltaT();
   reducto(&rN, pN, gCtx.org.n1F);
   reducto(&rA, pA, gCtx.org.n1F);
   dt= deltaT();
   //if ((rN.sum != gM) || (rA.sum != gM)) { 
   if (f & 1) { printf("sums: N=%G A=%G\n", rN.sum, rA.sum); }
   pMM->vMin= fmin(rN.mm.vMin, rA.mm.vMin);
   pMM->vMax= fmin(rN.mm.vMax, rA.mm.vMax);
   if (f & 1) { printf("MM: N=%G,%G A=%G,%G %Gsec\n", rN.mm.vMin, rN.mm.vMax, rA.mm.vMin, rA.mm.vMax, dt); }
} // redCheck

int main (int argc, char *argv[])
{
   int r= 0;

   if (init(&gCtx,1<<8))
   {
      const Index zSlice= gCtx.org.def.z / 2;
      uint iT=0, iN= 0, iA= 0;
      U8 nHoods[4]={6,14,18,26};
      Test1Param param;
      Test1Res   res[4][5], *pR;
      MapInfo mi;

      //setDefaultMap(gCtx.pM, &(gCtx.org.def));
      mapFromU8Raw(gCtx.pM, &mi, &(gCtx.ws), "s(256,256,256)u8.raw", 112, &(gCtx.org));
      //test(&(gCtx.org));
      //pragma acc set device_type(acc_device_none)
      //initW(gCtx.wPhase[0].w, 0.5, 6, 0); // ***M8***
      //iT= diffProcIsoD3S6M(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), (D3S6IsoWeights*)(gCtx.wPhase), gCtx.pM, 100);

      for (int h=0; h<sizeof(nHoods); h++)
      {
         pR= res[h];
         param.rD=    0.5;
         param.nHood= nHoods[h];
         for (int i=20; i<=100; i+= 20)
         {
            param.iter=  i;
            iT= test1(pR, &param, &mi);
            if (100 == iT)
            {
               MMSMVal mm;

               printf("diffProc..%u %u iter %G sec (%G msec/iter) Dt=%G sad=%G\n", param.nHood, iT, pR->tProc, 1000*pR->tProc / iT, pR->sr.Dt, pR->sr.sad);
               iN= iT & 1;
               iA= iN ^ 1;
               initAnalytic(gCtx.pSR[iA], &(gCtx.org), gM, pR->sr.Dt);
               redCheck(&mm, gCtx.pSR[iN], gCtx.pSR[iA], 1);
               {
                  char path[256];
                  snprintf(path, sizeof(path)-1, "%s/N%02d%s", "rgb", param.nHood, "numerical.rgb");
                  saveSliceRGB(path, gCtx.pSR[iN], 0, zSlice, &(gCtx.org), &mm);
                  snprintf(path, sizeof(path)-1, "%s/N%02d%s", "rgb", param.nHood, "analytic.rgb");
                  saveSliceRGB(path, gCtx.pSR[iA], 0, zSlice, &(gCtx.org), &mm);
                  snprintf(path, sizeof(path)-1, "%s/N%02d%s", "rgb", param.nHood, "sad.rgb");
                  saveSliceRGB(path, gCtx.ws.p, 0, 0, &(gCtx.org), NULL);
               }
            }
            pR++;
         }
      }
      //const SMVal tA= 0.05, rA= 1.0 / tA;
      printf("NH I\tt(sec)\tDt\tSAD\t\tAcc\tEff(Dt/t)\n");
      for (int h=0; h<sizeof(nHoods); h++)
      {
         pR= res[h];
         for (int i=20; i<=100; i+= 20)
         {
            printf("%2u %3u\t%5.03G\t%5.03G\t%7.05G\t%5.03G\t%5.03G\n", nHoods[h], i, pR->tProc, pR->sr.Dt, pR->sr.sad, gM-pR->sr.sad, pR->sr.Dt / pR->tProc);
            pR++;
         }
      }
   }
   release(&gCtx);
   printf("Complete\n");
   return(r);
} // main
