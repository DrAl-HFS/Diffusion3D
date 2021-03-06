// diffTest.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-June 2019

#include "diffTestUtil.h"
#include "diffTestHacks.h"

typedef struct
{
   DiffOrg        org;
   DiffScalar     *pSR[2];
   D3IsoWeights   wPhase[2];
   void           *pM, *pMC;
   MemBuff        ws;
} DiffTestContext;

//
typedef struct
{
   U16 def[5];
   U8  nDef, elemBytes;
} DataOrg;

typedef struct
{
   const char *pathName;
   const char *name;
   size_t      bytes;
} FileInfo;

typedef struct
{
   FileInfo file;
   DataOrg  org;
   int       isoDef, maxIter, stepIter, nSample;
} ArgInfo;

static DiffTestContext gCtx={0,};

static MapSiteInfo gMSI;

static ArgInfo gAI={0,};


/***/

U8 scanDef (U16 d[3], const char s[])
{
   int i= 0;
   U8 n=0;
   while (s[i] && !isdigit(s[i])) { ++i; }

   n+= sscanf(s+i,"%u",d+n);
   d[1]= d[2]= 0;
   return(n);
} // scanNameDef

Bool32 getOrg (DataOrg *pDO, const FileInfo *pFI, U8 verbose)
{
   pDO->nDef= scanDef(pDO->def, pFI->name);
   if (pDO->nDef > 0)
   {
      const U16 l= pDO->def[pDO->nDef-1];
      size_t x= pDO->def[0];
      for (U8 i=1; i < pDO->nDef; i++) { x*= pDO->def[i]; }

      while ((x < pFI->bytes) && (pDO->nDef < 3))
      {
         size_t d= pFI->bytes / x;
         //printf("%zu / %zu = %zu\n", pFI->bytes, x, d);
         if (d >= l)
         {
            pDO->def[ pDO->nDef++ ]= l;
            x*= l;
         }
      }
      // Pad to 3D
      while (pDO->nDef < 3) { pDO->def[ pDO->nDef++ ]= 1; }
      pDO->elemBytes= pFI->bytes / x;
      if (verbose)
      {
         LOG("getOrg(%s) - [%u]=", pFI->name, pDO->nDef);
         for (U8 i=0; i < pDO->nDef; i++) { printf("%u ", pDO->def[i]); }
         LOG("\n * %u = %zu (%zu)\n", pDO->elemBytes, x, pFI->bytes);
      }
      return(x <= pFI->bytes);
   }
   return(FALSE);
} // getOrg

void setDefaultDO (DataOrg *pO, const int d)
{
   if (0 == pO->nDef)
   {  
      pO->nDef= 3;
      pO->def[0]= pO->def[1]= pO->def[2]= d;
      pO->elemBytes= 8;
   }
} // setDefaultDO

int getVal (int *pR, const char *pCh)
{
   char *pE=NULL;
   long int t= strtol(pCh, &pE, 0);

   if (pE) { *pR= t; return(pE-pCh); } // # digits?
   return(0);
} // getVal

void scanArgs (ArgInfo *pAI, char *v[], const int n, U8 verbose)
{
   if (pAI->nSample <= 0) { pAI->nSample= 20; }
   if (pAI->isoDef <= 0) { pAI->isoDef= 256; }
   if (pAI->maxIter <= 0) { pAI->maxIter= 100; }
   if (pAI->stepIter <= 0) { pAI->stepIter= 10; }
   for (int i=0; i<n; i++)
   {
      char *pC= v[i];
      int n;
      if ('-' == pC[0]) switch(pC[1])
      {

         case 'S' : n= getVal(&(pAI->nSample),pC+2); break;
         case 'D' : n= getVal(&(pAI->isoDef),pC+2); break;
         case 'I' :
            pC+= 2;
            n= getVal(&(pAI->maxIter),pC);
            if (n > 0)
            {
               n+= (',' == pC[n]);
               pC+= n;
               n= getVal(&(pAI->stepIter), pC);
            }
            break;
      }
      else if ( isdigit(pC[0]) )
      {  // retained for old script compatibility
         int t;
         sscanf(pC,"%d",&t);
         if (t >= 0) { pAI->maxIter= t; }
      }
      else
      {
         size_t bytes= fileSize(pC);
         if (bytes > 0)
         {
            pAI->file.bytes= bytes;
            pAI->file.pathName= pC;
            pAI->file.name=     stripPath(pC);
            getOrg(&(pAI->org), &(pAI->file), verbose);
         }
      }
   }
   if (NULL == pAI->file.name) { setDefaultDO(&(pAI->org), pAI->isoDef); }
} // scanArgs

Bool32 init (DiffTestContext *pC, U16 def[3])
{
   initHack();
   if (initDiffOrg(&(pC->org), def, 1))
   {
      size_t b1M= pC->org.n1F * sizeof(D3MapElem); //*(pC->pM));
      size_t b1B= pC->org.n1B * sizeof(*(pC->pSR));
      size_t b1W= 8 * b1M; //pC->org.n1F * sizeof(*(pC->pSR));
      U32 nB= 0;

      pC->pM= malloc(b1M);
      pC->pMC= (D3S6MapElem*)(pC->pM) + pC->org.n1F; // NB only valid for M8
      pC->ws.bytes=  b1W; // 1<<28; ???
      pC->ws.p=  malloc(pC->ws.bytes);
      for (int i= 0; i<2; i++)
      {
         pC->pSR[i]= malloc(b1B);
         nB+= (NULL != pC->pSR[i]);
      }
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


#define PSM_ID_HOST   (0)
#define PSM_ID_CUDA   (1)
#define PSM_ID_KERN   (2)
#define PSM_ID_PLAIN  (3)
#define PSM_NCAT  (4)
#define PSM_NMCAT (2)
typedef struct // Performance
{
   StatMomD1R2 sm[PSM_NCAT];
   FBuf raw[PSM_NCAT];
   FBuf mes[PSM_NMCAT];
   void *p;
} PerfTestRes;

void setNF64 (F64 r[], const int n, F64 v) { for (int i=0; i<n; i++) { r[i]= v; } }
Bool32 initPTR (PerfTestRes *pPTR, const int nS, const int nM)
{
   int n= nS * PSM_NCAT + nM * 2;
   float *pF= malloc(sizeof(*pF) * n);
   //LOG_CALL("(.. %d,%d) -> %d %p\n", nS, nM, n, pF);
   if (pF)
   {
      pPTR->p= pF;
      for (int i= 0; i<PSM_NCAT; i++)
      {
         setNF64(pPTR->sm[i].m, 3, 0);
         pPTR->raw[i].pF=   pF;
         pPTR->raw[i].maxF= nS;
         pPTR->raw[i].nF=   0;
         pF+= nS;
      }
      for (int i= 0; i<PSM_NMCAT; i++)
      {
         pPTR->mes[i].pF= pF;
         pPTR->mes[i].maxF= nM;
         pPTR->mes[i].nF=   0;
         pF+= nM;
      }
      return(TRUE);
   }
   return(FALSE);
} // initPTR

void addObs (PerfTestRes *pR, const int c, const float v)
{
   statMom1Add(pR->sm+c, v);
   addFB(pR->raw+c, v);
} // addObs

#include "hostFMA.h"

void deriveRadii (float r[3], const float mkf[4])
{
   const float s= 1.0 / (4 * M_PI);
   r[0]= mkf[1] * s;
   r[1]= sqrt(mkf[2] * s);
   r[2]= pow(3 * mkf[3] * s, 1.0/3);
} // deriveRadii

//strExtFmtNSMV(" m=(",")")
void testPerfMKF (PerfTestRes *pR, const TestParam *pP)
{
   HostFMA  hostFMA={0};
   int iT=0, iR, iM;
   float dt, dr[3], mkf[4]={0,};
   const char oprStr[]=">";
   const float fScale= 3.0 / sumNI(&(gCtx.org.def.x), 3);  // reciprocal mean
   const float tD= pow(0.5, 0.90 * pP->iter); //1.0 / (256.0 * prodNI(&(gCtx.org.def.x), 3));
   const size_t fieldBytes= sizeof(DiffScalar) * gCtx.org.n1B;
   size_t refBytes= 0;

   if (initIsoW(gCtx.wPhase[0].w, pP->rD, pP->nHood, 0) > 0)
   {
      const int maxM= 1+DIV_RUP(pP->iter, pP->mIvl);
      
      LOG("maxM=%d\n", maxM);
      // important to warm up
      initFieldVCM(gCtx.pSR[0], &(gCtx.org), NULL, NULL, &gMSI);
      setupAcc(1,0); // Ensure GPU ACC for diffusion.
      diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, 10, pP->nHood);
      LOG_CALL("() - warmup complete\tStarting %d samples/Category. Iterations: %d total, %d measure interval\n", pP->samples, pP->iter, pP->mIvl);
      
      // Baseline
      LOG("*CAT: %s Func.\n","No");
      for (int iS=0; iS<pP->samples; iS++)
      {
         iT= 0;
         initFieldVCM(gCtx.pSR[0], &(gCtx.org), NULL, NULL, &gMSI);
         deltaT();
         iT= diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, pP->iter, pP->nHood);
         dt= deltaT();
         addObs(pR, PSM_ID_PLAIN, dt);
         LOG("s%d %G\n", iS, dt);
         iR= iT & 0x1;
      }
      if (refBytes && gCtx.ws.p && (gCtx.ws.bytes >= fieldBytes))
      {  // Copy baseline result buffer for sanity check
         LOG("memcpy(%p, %p, %zu)\n", gCtx.ws.p, gCtx.pSR[iT&0x1], fieldBytes);
         memcpy(gCtx.ws.p, gCtx.pSR[iR], fieldBytes);
         refBytes= fieldBytes;
      }

      // OpenACC - host
      if (hostSetupFMA(&hostFMA, oprStr, tD, fScale, &(gCtx.org)))
      {
         const float rScale = 1.0 / fScale;
         hostAnalyse(mkf, &hostFMA, gCtx.pSR[iR]); 
         LOG("Baseline post-proc measures: %G %G %G %G\t(tD=%G)\n", mkf[0], mkf[1], mkf[2], mkf[3], tD);
         deriveRadii(dr, mkf);
         LOG("Derived radii: rB=%G rS=%G rV=%G\t(=%.1f %.1f %.1f lu)\n", dr[0], dr[1], dr[2], dr[0]*rScale, dr[1]*rScale, dr[2]*rScale);
         LOG("rS/rB=%.2f\trV/rS=%.2f\t\n\n", dr[1]/dr[0], dr[2]/dr[1]);

         LOG("*CAT: %s Func.\n","Host");
         for (int iS=0; iS<pP->samples; iS++)
         {
            iT= 0;
            initFieldVCM(gCtx.pSR[0], &(gCtx.org), NULL, NULL, &gMSI);
            deltaT();
            while (iT < pP->iter)
            {
               iR= pP->iter - iT;
               iM= MIN(pP->mIvl, iR);
               iM= MAX(1, iM);
               iR= iT & 0x1; // previous result buffer index
               setupAcc(1,0); // Switch to GPU ACC for diffusion...
               iT+= diffProcIsoD3SxM(gCtx.pSR[iR^0x1], gCtx.pSR[iR], &(gCtx.org), gCtx.wPhase, gCtx.pM, iM, pP->nHood);
               setupAcc(0,0); // ...host ACC for analysis
               iR= iT & 0x1;
               hostAnalyse(allocNF(pR->mes+PSM_ID_HOST, 4), &hostFMA, gCtx.pSR[iR]);
            }
            dt= deltaT();
            addObs(pR, PSM_ID_HOST, dt);
            LOG("s%d %G\n", iS, dt);
         }
         hostTeardownFMA(&hostFMA);
      }
      if (refBytes > 0)
      {
         iR= iT & 0x1;
         LOG("***Baseline-HostFMA memcmp() -> %d\n", memcmp(gCtx.ws.p, gCtx.pSR[iR], refBytes));
      }

      // CUDA
      if (diffSetupFMA(maxM, oprStr, tD, fScale, &(gCtx.org)))
      {
         diffSetIntervalFMA(pP->mIvl);
         LOG("*CAT: %s Func.\n","CUDA");
         for (int iS=0; iS<pP->samples; iS++)
         {
            iT= 0;
            initFieldVCM(gCtx.pSR[0], &(gCtx.org), NULL, NULL, &gMSI);
            deltaT();
            iT= diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, pP->iter, pP->nHood);
            dt= deltaT();
            addObs(pR, PSM_ID_CUDA, dt);
            LOG("s%d %G\n", iS, dt);
            FMAPkt *pK; int nK;
            nK= diffGetFMA(&pK, TRUE);
            if (nK > 0)
            {
               FMAPkt *pL= pK + nK-1;
               LOG("\ti%d: %G %G %G %G\t(kt=%G+%G)\n", pL->i, pL->m[0], pL->m[1], pL->m[2], pL->m[3], pL->dt[0], pL->dt[1]);
               dt= 0;
               for (int iK= 0; iK < nK; iK++)
               {
                  float *pF= allocNF(pR->mes+PSM_ID_CUDA, 4);
                  if (pF) { memcpy(pF, pK->m, sizeof(pK->m)); }
                  dt+= pK->dt[0] + pK->dt[1];
                  pK++;
               }
               addObs(pR, PSM_ID_KERN, dt);
            }
         }
         diffTeardownFMA();
      }
      if (refBytes > 0)
      {
         iR= iT & 0x1;
         LOG("***Baseline-CUDA memcmp() -> %d\n", memcmp(gCtx.ws.p, gCtx.pSR[iR], refBytes));
      }
   }
   {
      int t= 0;
      for (int i=0; i<PSM_NCAT; i++) { t+= pR->sm[i].m[0]; }
      LOG_CALL("() - complete %d samples\n", t);
   }
} // testPerfMKF

typedef struct // Analytic Comparison
{
   SMVal          tProc;
   SearchResult  sr;
} ACTestRes;

U32 testAn (ACTestRes *pR, const TestParam *pP)
{
   U32 iT=0, iN= 0, iA= 0;

   if (initIsoW(gCtx.wPhase[0].w, pP->rD, pP->nHood, 0) > 0)
   {
      initFieldVCM(gCtx.pSR[0], &(gCtx.org), NULL, NULL, &gMSI);
      deltaT();
      iT= diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, pP->iter, pP->nHood);
      pR->tProc= deltaT();
      iN= iT & 1;

      // Estimate Diffusion-time moment
      AnResD3R2 a;
      analyseField(&a, gCtx.pSR[iN], &(gCtx.org));

      SMVal isovar= meanNSMV(a.var, 3);
      SMVal Dt= searchMin1(&(pR->sr), &(gCtx.ws), gCtx.pSR[iN], &(gCtx.org), gMSI.v, 0.5 * isovar, 0);

      char txt[256];
      const int m= sizeof(txt)-1;
      int n= 0;
      n= strFmtNSMV(txt, m, "%.4G,", a.mean, 3);
      printf("m=(%s)\n", txt);
      n= strFmtNSMV(txt, m, "%.4G,", a.var, 3);
      printf("v=(%s)\n", txt);
//m=(%.4G,%.4G,%.4G) v=(%.4G,%.4G,%.4G)
//a.m[0], a.m[1], a.m[2], a.v[0], a.v[1], a.v[2],
      printf("analyseField() - mm=%G,%G sum=%G isoVar=%G Dt=%G\n", a.mm.vMin, a.mm.vMax, a.sum, isovar, Dt);
   }
   return(iT);
} // testAn

void redCheck (MMSMVal *pMM, const DiffScalar *pN, const DiffScalar *pA, U32 f)
{
   RedRes rrN, rrA;
   SMVal dt;

   deltaT();
   reduct0(&rrN, pN, gCtx.org.n1F);
   reduct0(&rrA, pA, gCtx.org.n1F);
   dt= deltaT();
   if (f & 1) { printf("sums: N=%G A=%G\n", rrN.sum, rrA.sum); }
   pMM->vMin= fmin(rrN.mm.vMin, rrA.mm.vMin);
   pMM->vMax= fmin(rrN.mm.vMax, rrA.mm.vMax);
   if (f & 1) { printf("MM: N=%G,%G A=%G,%G %Gsec\n", rrN.mm.vMin, rrN.mm.vMax, rrA.mm.vMin, rrA.mm.vMax, dt); }
} // redCheck

void save (const char *basePath, const char *baseName, const DiffScalar *pS, const U32 nHood, const U32 iter, const int z, const MMSMVal *pMM)
{
   char path[256];
   const int m= sizeof(path)-1;
   int n= 0;

   if (z >= 0)
   {
      n+= snprintf(path+n, m-n, "%s/N%02d_I%07d_Z%03d_%s", basePath, nHood, iter, z, baseName);
      saveSliceRGB(path, pS+dotS3(0,0,z,gCtx.org.stride), &(gCtx.org), pMM);
   }
   else
   {
      n+= snprintf(path+n, m-n, "%s/N%02d_I%07d_%s", basePath, nHood, iter, baseName);
      saveSliceRGB(path, pS, &(gCtx.org), pMM);
   }
} // save

void compareAnNHI (const U8 nHoods[], const U8 nNH, const U32 step, const U32 max)
{
   TestParam param;
   ACTestRes  res[4][5], *pR;
   U32        iT, iN, iA;

#ifdef  DIFF_FUMEAN
   FMAPkt *pK; int nK;
   diffSetupFMA(max, ">", 0, 3.0/sumNI(&(gCtx.org.def.x), 3), &(gCtx.org));
#endif
   for (U8 h=0; h<nNH; h++)
   {
      param.nHood= nHoods[h];
      pR= res[h];
      param.rD=    0.5;
      // warm-up (field not initialised)
      initIsoW(gCtx.wPhase[0].w, param.rD, param.nHood, 0);
      diffSetIntervalFMA(-1);
      diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, 2, param.nHood);
      diffSetIntervalFMA(10);
      printf("diffProc.. warmup OK\n");
      for (int i=step; i<=max; i+= step)
      {
         param.iter=  i;
         iT= testAn(pR, &param);

#ifdef  DIFF_FUMEAN
         nK= diffGetFMA(&pK, TRUE);
         if (nK > 0)
         {
            LOG("getAnalysis() - %d %p (I:%u)\n", nK, pK, param.iter);
            for (int iK= 0; iK < nK; iK++)
            {
               LOG("\ti%d %G %G %G %G\t%G %G\n", pK->i, pK->m[0], pK->m[1], pK->m[2], pK->m[3], pK->dt[0], pK->dt[1]);
               pK++;
            }
         }
#endif // DIFF_FUMEAN

         if (iT >= max)
         {
            const Index zSlice= gCtx.org.def.z / 2;
            MMSMVal mm;

            printf("diffProc..%u %u iter %G sec (%G msec/iter) Dt=%G sad=%G\n", param.nHood, iT, pR->tProc, 1000*pR->tProc / iT, pR->sr.Dt, pR->sr.sad);
            iN= iT & 1;
            iA= iN ^ 1;
            initAnalytic(gCtx.pSR[iA], &(gCtx.org), gMSI.v, pR->sr.Dt);
            //redCheck(&mm, gCtx.pSR[iN], gCtx.pSR[iA], 1);
            save("rgb", "NUM.rgb", gCtx.pSR[iN], param.nHood, param.iter, zSlice, NULL);
            save("rgb", "ANL.rgb", gCtx.pSR[iA], param.nHood, param.iter, zSlice, NULL);
            save("rgb", "SAD.rgb", gCtx.ws.p, param.nHood, param.iter, -1, NULL);
         }
         pR++;
      }
   }
   //const SMVal tA= 0.05, rA= 1.0 / tA;
   printf("NH I\tt(sec)\tDt\tSAD\t\tAcc\tEff(Dt/t)\n");
   for (U8 h=0; h<nNH; h++)
   {
      pR= res[h];
      for (int i=step; i<=max; i+= step)
      {
         printf("%2u %3u\t%5.03G\t%5.03G\t%7.05G\t%5.03G\t%5.03G\n", nHoods[h], i, pR->tProc, pR->sr.Dt, pR->sr.sad, gMSI.v-pR->sr.sad, pR->sr.Dt / pR->tProc);
         pR++;
      }
   }
   diffTeardownFMA();
} // compareAnNHI


void scaleV3I (V3I *pR, const V3I *pS, const float s)
{
   pR->x= pS->x * s;
   pR->y= pS->y * s;
   pR->z= pS->z * s;
} // scaleV3I

U32 testMap (RedRes *pRR, const TestParam * pTP, const MapDesc * pMD, U32 iT, const char *baseName)
{
   RedRes rrN;
   U32    iN=0, testV=0, dumpR=0;

   iN= iT & 1;
   initFieldVCM(gCtx.pSR[iN], &(gCtx.org), NULL, NULL, &gMSI);
   if (dumpR > 0) { dumpSMR(gCtx.pSR[iN], gCtx.pM, &(gMSI.c), dumpR, &(gCtx.org)); }

   reduct3_2_0(&rrN, gCtx.ws.p, gCtx.pSR[iN], &(gCtx.org), 0);
   printf("Initial SMM: %G, %G, %G\n", rrN.sum, rrN.mm.vMin, rrN.mm.vMax );
   // Set NAN / -1 for test
   if (testV > 0) { resetFieldVCM(gCtx.pSR[iN], &(gCtx.org), gCtx.pM, &(gDefObsKV.k), gDefObsKV.v2[1]); }

   initIsoW(gCtx.wPhase[0].w, 0.5, pTP->nHood, 0);
   switch (pMD->mapElemBytes)
   {
      case 1:
         // if (6 != pMD->nHood ) ERROR(...);
         iT+= diffProcIsoD3S6M8(gCtx.pSR[iN^1], gCtx.pSR[iN], &(gCtx.org), (D3S6IsoWeights*)gCtx.wPhase, gCtx.pM, pTP->iter);
         break;
      case 4:
         iT+= diffProcIsoD3SxM(gCtx.pSR[iN^1], gCtx.pSR[iN], &(gCtx.org), gCtx.wPhase, gCtx.pM, pTP->iter, pTP->nHood);
         break;
   }
   iN= iT & 1;

   // Clear NAN / -1 for tally
   if (testV > 0) { resetFieldVCM(gCtx.pSR[iN], &(gCtx.org), gCtx.pM, NULL, 0); }
   if (dumpR > 0) { dumpSMR(gCtx.pSR[iN], NULL, &(gMSI.c), dumpR, &(gCtx.org)); }

   const char mode= 0;
   reduct3_2_0(&rrN, gCtx.ws.p, gCtx.pSR[iN], &(gCtx.org), mode);
   printf("N%u I%u SMM: %G, %G, %G\n", pTP->nHood, iT, rrN.sum, rrN.mm.vMin, rrN.mm.vMax );
   if (pRR) { *pRR= rrN; }

   if (baseName)
   { // output
      MMSMVal mm, *pMM=NULL;
      if ('L' == mode)
      {
         mm.vMax= log(rrN.mm.vMax);
         if (rrN.mm.vMin > 0) { mm.vMin= log(rrN.mm.vMin); } else { mm.vMin= log(gEpsilon); }
         pMM= &mm;
      } else { pMM= &(rrN.mm); }

      save("rgb", baseName, gCtx.ws.p, pTP->nHood, pTP->iter, -1, pMM);
   }
   return(iT);
} // testMap

void dumpPerfTestRes (const PerfTestRes *pR)
{
   const char *catLab[PSM_NCAT]={"HFHT","CFHT","CFKT","NFHT"};
   StatResD1R2 sr;
   int iC;

   report(OUT,"\n\nCat.\tMean\tVar.");
   for (iC= 0; iC<PSM_NCAT; iC++)
   {
      if (statMom1Res1(&sr, pR->sm+iC, 1) > 0)
      {
         report(OUT,"\n%s\t%6G\t%6G", catLab[iC], sr.m, sr.v);
      }
   }
   report(OUT,"\n\nCat.\tN.Smpl\tRaw...");
   for (iC= 0; iC<PSM_NCAT; iC++)
   {
      report(OUT,"\n%s\t%d", catLab[iC], pR->raw[iC].nF);
      for (int iR= 0; iR<pR->raw[iC].nF; iR++)
      {
         report(OUT,"\t%6G", pR->raw[iC].pF[iR]);
      }
   }
   iC= 0;   // Get first category with measures
   while ((iC < PSM_NMCAT) && (pR->mes[iC].nF <= 0)) { iC++; }
   if (iC < 2)
   {
      int i= iC;
      report(OUT,"\n\n%s K M S V", catLab[i]);
      while (++i < PSM_NMCAT)
      {
         if (pR->mes[i].nF > 0) { report(OUT,"\t\t\t\t\t%s K M S V", catLab[i]); }
      }
      for (int iM= 0; (iM+4) <= pR->mes[iC].nF; iM+= 4)
      {
         const float *pM= pR->mes[iC].pF + iM;
         report(OUT,"\n%6G\t%6G\t%6G\t%6G", pM[0], pM[1], pM[2], pM[3]);
         i= iC;
         while (++i < PSM_NMCAT)
         {
            if (pR->mes[i].nF >= (iM+4))
            {
               pM= pR->mes[i].pF+iM;
               report(OUT,"\t\t%6G\t%6G\t%6G\t%6G", pM[0], pM[1], pM[2], pM[3]);
            }
         }
      }
   }
   report(OUT,"\n\n");
} // dumpPerfTestRes

int main (int argc, char *argv[])
{
   MapDesc md={0};
   int r= 0;

   scanArgs(&gAI, argv+1, argc-1, TRUE);
   //utilTest();
   if (init(&gCtx, gAI.org.def))
   {
      float f=-1;
      U32 mapID= 0;
      gMSI.v= 1.0;

      if (gAI.file.bytes > 0)
      {
         RawTransMethodDesc rm={0};

         getProfileRM(&rm, TFR_ID_PDFPERM, MAP_ID_B1NH6, D3UF_NONE); // D3UF_PERM_SAVE|D3UF_CLUSTER_SAVE|D3UF_CLUSTER_TEST
         f= mapFromU8Raw(gCtx.pM, &md, &(gCtx.ws), gAI.file.pathName, &rm, &(gCtx.org));
         if (f > 0) { gMSI.c= md.site; }
         //printf("site= (%d,%d,%d)\n", gMSI.c.x, gMSI.c.y, gMSI.c.z);
         mapID= -1;
      }
      if (mapID <= 1)
      {
         f= setDefaultMap(gCtx.pM, &md, &(gCtx.org.def), mapID);
         scaleV3I(&(gMSI.c), &(gCtx.org.def), 0.5);
      }

      report(LOG0,"\nmain() - initialisation complete\n----\n");
      //pragma acc set device_type(acc_device_none) no effect ???

      //initW(gCtx.wPhase[0].w, 0.5, 6, 0); // ***M8***
      //iT= diffProcIsoD3S6M(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), (D3S6IsoWeights*)(gCtx.wPhase), gCtx.pM, 100);

      if (0 == mapID) // && (4 == md.mapElemBytes))
      {
#if 1
         TestParam   param;   // TODO - parameterise from CLI
         PerfTestRes res={0};

         param.nHood= 6;
         param.iter=  gAI.maxIter;
         param.rD=    0.5;
         param.mIvl=  gAI.stepIter;
         param.samples= gAI.nSample;
         initPTR(&res, param.samples, 4 * (1 + DIV_RUP(param.iter, param.mIvl)));
         testPerfMKF(&res, &param);

         report(OUT,"D%d N%d I%d,%d S%d\n", gAI.isoDef, param.nHood, param.iter, param.mIvl, param.samples);
         dumpPerfTestRes(&res);

         if (res.p) { free(res.p); memset(&res, 0, sizeof(res)); }
#else
         static const U8 nHoods[]={6};//,14,18,26};
         compareAnNHI(nHoods, sizeof(nHoods), 20, 100);
#endif
      }
      else
      {
         TestParam  param={0,};
         U32        iT=0, iN=0;
         RedRes    rr;

         //iT= 0; iN= iT & 1;
         param.nHood=md.nHood;
         param.iter= 50; //if (mapID > 0) { param.iter= 200; }
         param.rD=   0.5;
         iT= testMap(&rr, &param, &md, iT, "NRED.rgb");
         iN= iT & 1;
         LOG("rr: s=%G um=%G mm=%G,%G\n", rr.sum, rr.uMin, rr.mm.vMin, rr.mm.vMax);

         // dependant diffusion test
         if (1 == md.mapElemBytes)
         {
            DiffScalar *pS= gCtx.pSR[iN];
            DupConsParam dcp={1E-15,1E-15};//lerpF64(rr.uMin, rr.mm.vMax, 1E-15);
            size_t n= map8DupCons(gCtx.pMC, gCtx.pM, &md, &(gCtx.org), pS, pS, &dcp);

            n= gCtx.org.n1F * sizeof(*pS);
            pS= malloc(n);
            if (pS)
            {
               memcpy(pS, gCtx.pSR[iN], n);
               SWAP(void*, gCtx.pMC, gCtx.pM);

               //param.iter= 200;
               iT= testMap(&rr, &param, &md, 0, "NDEP.rgb");
               iN= iT & 1;
               LOG("rr: s=%G um=%G mm=%G,%G\n", rr.sum, rr.uMin, rr.mm.vMin, rr.mm.vMax);

               // swap back for cleanup
               SWAP(void*, gCtx.pMC, gCtx.pM);
               free(pS);
            }
         }
      }
   }
   release(&gCtx);
   report(LOG0,"Complete\n");
   return(r);
} // main
