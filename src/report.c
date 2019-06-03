// report.c - GSRD/DIFF3D
// https://github.com/DrAl-HFS/ .git
// (c) Project Contributors  Feb 2018 - June 2019


#include "report.h"
#include <stdarg.h>

typedef struct
{
   U8 filterMask[4];
} ReportCtx;

static ReportCtx gRC={0xFF,0xFF,0xFF,0xFF};


/***/

static const char * getPrefix (U8 tid)
{
static const char *pfx[]=
{
   NULL,
   "TRACE",
   "WARNING",
   "ERROR"
};
   return( pfx[ tid&0x3 ] );
} // getPrefix

int report (U8 id, const char fmt[], ...)
{
   int r=0;
   va_list args;
   // NB: stdout & stderr are supposed to be macros (Cxx spec.) and so might
   // create safety/portability issues if not handled in the obvious way...
   if (OUT == id)
   {
      va_start(args,fmt);
      r+= vfprintf(stdout, fmt, args);
      va_end(args);
   }
   else
   {
      const U8 tid= (id & ERR) >> REP_MSK_BITS;
      const U8 lid= (id & 0x03);
      if (gRC.filterMask[tid] & (1 << lid))
      {
         const char *p= getPrefix(tid);
         if (p) { r+= fprintf(stderr, "%s: ", p); }
         va_start(args,fmt);
         r+= vfprintf(stderr, fmt, args);
         va_end(args);
      }
   }
   //else user log/result files?
   return(r);
} // report

int reportN (U8 id, const char *a[], int n, const char start[], const char sep[], const char *end)
{
   int i,r=0;
   if ((OUT == id) && (n > 0))
   {
      r= fprintf(stdout, "%s%s", start, a[0]);
      for (i=1; i<n; i++) { r+= fprintf(stdout, "%s%s", sep, a[i]); }
      if (end) { r+= fprintf(stdout, "%s", end); }
   }
   return(r);
} // reportN


