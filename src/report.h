// report.c - GSRD/DIFF3D
// https://github.com/DrAl-HFS/ .git
// (c) Project Contributors  Feb 2018 - June 2019

#ifndef REPORT_H
#define REPORT_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REP_MSK_BITS (4)
#define OUT 0x00
#define TRC 0x10
#define WRN 0x20
#define ERR 0x30
// Upper two bits of each nybble reserved: 
// id is --xx--yy where xx=category bits yy=level bits

/***/

extern int report (U8 id, const char fmt[], ...);
 
extern int reportN (U8 id, const char *a[], int n, const char start[], const char sep[], const char *end);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // REPORT_H
