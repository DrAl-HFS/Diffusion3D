// clusterMap.c - Map representation of cluster data

#include "clusterMap.h"

static void analyseNH6 (const I32 * pNHNI, const size_t nNHNI)
{
   U32 hlb[6][32]={0,}, htb[128]={0,};

   for (size_t i= 0; i < nNHNI; i+= 6)
   {
      U32 b, bt=0;
      for (U8 l=0; l < 6; l++)
      {
         I32 d= pNHNI[i+l];
         b= 1 + bitsReqI32(d); // NB signed representation requires extra bit!
         bt+= b;
         hlb[l][b]++;
      }
      htb[bt]++;
   }
   report(TRC0,"analyseNH6() -\n");
   report(TRC1,"Raw bits per link dist. :\n");
   for (U32 b=0; b<32; b++)
   {
      U32 s= 0;
      for (U8 l=0; l < 6; l++) { s+= hlb[l][b]; }
      if (s > 0)
      {
         report(TRC1,"%2u: ",b);
         for (U8 l=0; l < 6; l++) { report(TRC1,"%8u ",hlb[l][b]); }
         printf("\n");
      }
   }
   printf("Raw bits per site dist. :\n");
   for (U32 b=0; b < 128; b++) { if (htb[b] > 0) { report(TRC1,"%2u: %8u\n", b, htb[b]); } }
   report(LOG1,"\n");
} // analyseNH6

// NB - discards original ordering
I32 offsetArrange (U8 *pSignM, I32 t[6], const I32 o[6])
{
   U8 h, nz=0, signM=0;
   memcpy(t, o, 6*sizeof(I32));
   clusterSortAbsID(t, 6); // largest first: 654321

   while ((nz < 6) && (0 != t[nz])) { nz++; }
   for (h=0; (h+1) < nz; h+=2) SWAP(I32, t[h], t[h+1]); // pair swap: 563412

   for (h=0; h < nz; h++)
   {  // separate sign
      if (t[h] < 0) { t[h]= -t[h]; signM|= (1<<h); }
   }
   *pSignM= signM;

   for (h=0; (h+1) < nz; h+=2) { t[h+1]-= t[h]; } // pair delta: 5,(6-5),3,(4-3),1,(2-1)

   return(nz);
} // offsetArrange
 
typedef size_t NH6Pkt;
size_t compressNH6 (MemBuff *pWS, const I32 *pNHNI, const U32 nNI, U8 verbose)
{
   size_t bytes= nNI*sizeof(NH6Pkt);
   if (validMemBuff(pWS,bytes))
   {
      U32 hlb[6][32]={0,}, htb[128]={0,};
      U8 signM; // 6 sign bits
      I32 d[6]; // transformed offsets
      U32 mbt=0;

      if (verbose) { report(TRC0,"compressNH6() -\n\tIdx:\tSGNF\tL0..5\n"); }
      for (size_t i= 0; i < nNI; i++)
      {
         U32 b, bt=0;
         offsetArrange(&signM, d, pNHNI+i*6);
         for (U8 l=0; l<6; l++)
         {
            U8 b= bitsReqI32(d[l]);
            bt+= b;
            hlb[l][b]++;
         }
         if (verbose && (bt > mbt))
         {
            mbt= bt;
            report(TRC1,"%8zu: ", i);
            //for (h=0; h < 6; h++) { report(LOG1," %+d", pNHNI[i*6+h]); }
            report(TRC1," -> 0x%02x ", signM);
            for (U8 l=0; l < 6; l++) { report(LOG1," %u", d[l]); }
            //printf(" ->");
            //for (h=0; h < 6; h++) { report(LOG1," %u", d[h]); }
            report(TRC1," (%u)\n", bt);
         }
         htb[bt]++;
      }
      report(TRC0,"\ncompressNH6() -\n");
      report(TRC1,"Trans. bits per link dist. :\n");
      for (U32 b=0; b<32; b++)
      {
         U32 s= 0;
         for (U8 l=0; l < 6; l++) { s+= hlb[l][b]; }
         if (s > 0)
         {
            report(TRC1,"%2u: ",b);
            for (U8 l=0; l < 6; l++) { report(TRC1,"%8u ",hlb[l][b]); }
            report(TRC1,"\n");
         }
      }
      report(LOG1,"Trans. bits per site dist. :\n");
      for (U32 b=0; b < 128; b++) { if (htb[b] > 0) { report(TRC1,"%2u: %8u\n", b, htb[b]); } }
      report(TRC1,"\n");
   }
   return(0);
} // compressNH6

size_t clusterReorderS (ClustIdx ni[], const size_t nNI)
{
   U32 s=0;
   for (size_t i=1; i<nNI; i++)
   {
      int d= (int)(ni[i]) - (int)(ni[i-1]);
      if (-1 == d) { SWAP(ClustIdx, ni[i], ni[i-1]); s++; }
   }
   report(TRC0,"clusterReorderS() - s=%u\n", s);
   return(s);
} // clusterReorderS

size_t clusterReorderM (MemBuff ws, ClustIdx ni[], const size_t nNI, ClustIdx *pM, const MapOrg *pO)
{
   U32 s=0;
   for (size_t i=2; i<nNI; i++)
   {
      int d1= (int)(ni[i]) - (int)(ni[i-1]);
      int d2= (int)(ni[i]) - (int)(ni[i-2]);
      if ((2 == d2) && (1 != d1))
      {
         ClustIdx q= ni[i-1];
         ClustIdx e= ni[i]-1;
         ClustIdx e1= pM[e];
         ClustIdx q1= pM[q];
         if (e1 > 0)
         {  // expected site exists
            int c= ni[i] % pO->stride[1]; //def.x;
            if (c >= 2) // within a row
            {  // swap in list and map
               SWAP(ClustIdx, pM[q], pM[e]);
               SWAP(ClustIdx, ni[q1], ni[e1]);
               s++;
            }
         }
      }
   }
   report(TRC0,"clusterReorderM() - s=%u\n", s);
   return(s);
} // clusterReorderM

void clusterMapTest (MemBuff ws, ClustIdx *pMaxNI, const size_t nNI, const U8 *pM, const MapOrg *pO)
{
   size_t bytes= pO->n * sizeof(ClustIdx);
   if (ws.bytes >= bytes)
   {  // Build spatial map for neighbour lookup, then calculate offsets per site
      ClustIdx *pIdxMap= ws.p;
      const U8 nNH= 6;
      char ch[2];
      U8 f= CLF_VERBOSE|CLF_REORDER1|CLF_REORDER2;

      if (f & CLF_REORDER1) { clusterReorderS(pMaxNI,nNI); }
      report(TRC0,"Building map... %p ", pIdxMap);
      memset(ws.p, 0, bytes);
      adjustMemBuff(&ws, &ws, bytes, 0);
      for (size_t i= 0; i < nNI; i++) { ClustIdx j= pMaxNI[i]; pIdxMap[ j ]= i; }
      report(TRC1,"%G%cbytes\n", binSizeZ(ch,bytes), ch[0]);
      if (f & CLF_REORDER2) { clusterReorderM(ws, pMaxNI, nNI, pIdxMap, pO); }

      bytes= nNI * nNH * sizeof(ClustIdx);
      if (ws.bytes >= bytes)
      {
         I32 *pNHNI= ws.p;
         U32 nNHNI= nNH * nNI, err=0,c=0;
         report(TRC1,"Computing offsets... %p ", pNHNI);
         memset(ws.p, 0, bytes);
         adjustMemBuff(&ws,&ws,bytes,0);
         for (size_t i= 0; i < nNI; i++)
         {
            const size_t k= i * nNH;
            const ClustIdx j= pMaxNI[i];
            const U8 m= pM[j];

            err+= (i != pIdxMap[j]);
            for (U8 h=0; h<nNH; h++)
            {
               if (m & (1<<h)) { pNHNI[k+h]= pIdxMap[ j + pO->step[h] ] - i; }
            }
         }
         report(TRC1,"%G%cbytes\n", binSizeZ(ch,bytes), ch[0]);
         if (err > 0) { report(ERR0,"index map corrupt (%u)\n", err); }

         report(TRC0,"Analysing...\n");
         analyseNH6(pNHNI, nNHNI);
         compressNH6(&ws,pNHNI,nNI,f&CLF_VERBOSE);

      } else report(ERR0,"offsetMapTest() - only %G%cbytes of %G%cbytes avail\n", binSizeZ(ch+0,ws.bytes), ch[0], binSizeZ(ch+1,bytes), ch[1]);
   }
} // clusterMapTest
