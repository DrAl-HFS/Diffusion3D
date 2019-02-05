// util.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "util.h"

#define GETTIME(a) gettimeofday(a,NULL)
#define USEC(t1,t2) (((t2).tv_sec-(t1).tv_sec)*1000000+((t2).tv_usec-(t1).tv_usec))

/***/

Bool32 validBuff (const MemBuff *pB, size_t b)
{
   return(pB && pB->p && (pB->bytes >= b));
} // validBuff

void releaseMemBuff (MemBuff *pB)
{
   if (pB && pB->p) { free(pB->p); pB->p= NULL; }
} // releaseMemBuff

const char *stripPath (const char *path)
{
   if (path && *path)
   {
      const char *t= path, *l= path;
      do
      {
         path= t;
         while (('\\' == *t) || ('/' == *t) || (':' == *t)) { ++t; }
         if (t > path) { l= t; }
         t += (0 != *t);
      } while (*t);
      return(l);
   }
   return(NULL);
} // stripPath

// const char *extractPath (char s[], int m, const char *pfe) {} // extractPath

size_t fileSize (const char * const path)
{
   if (path && path[0])
   {
      struct stat sb={0};
      int r= stat(path, &sb);
      if (0 == r) { return(sb.st_size); }
   }
   return(0);
} // fileSize

size_t loadBuff (void * const pB, const char * const path, const size_t bytes)
{
   FILE *hF= fopen(path,"r");
   if (hF)
   {
      size_t r= fread(pB, 1, bytes, hF);
      fclose(hF);
      if (r == bytes) { return(r); }
   }
   return(0);
} // loadBuff

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

SMVal deltaT (void)
{
   static struct timeval t[2]={0,};
   static int i= 0;
   SMVal dt;
   GETTIME(t+i);
   dt= 1E-6 * USEC(t[i^1],t[i]);
   i^= 1;
   return(dt);
} // deltaT

void statAddW (const StatMom * const pS, const SMVal v, const SMVal w)
{
   pS->m[0]+= w;
   pS->m[1]+= v * w;
   pS->m[2]+= v * v * w;
} // statAddW

U32 statGetRes1 (StatRes1 * const pR, const StatMom * const pS, const SMVal dof)
{
   StatRes1 r={ 0, 0 };
   U32 o= 0;
   if (pS && (pS->m[0] > 0))
   {
      r.m= pS->m[1] / pS->m[0];
      ++o;
      if (pS->m[0] != dof)
      { 
         //SMVal ess= (pS->m[1] * pS->m[1]) / pS->m[0];
         r.v= ( pS->m[2] - (pS->m[1] * r.m) ) / (pS->m[0] - dof);
         ++o;
      }
   }
   if (pR) { *pR= r; }
   return(o);
} // statGetRes1


#ifdef UTIL_TEST

int utilTest (void)
{
   return(0);
} // utilTest

#endif

#ifdef UTIL_MAIN

int main (int argc, char *argv[])
{
   return utilTest();
} // utilTest

#endif
