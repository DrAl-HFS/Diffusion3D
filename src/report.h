// report.c - GSRD/DIFF3D
// https://github.com/DrAl-HFS/ .git
// (c) Project Contributors  Feb 2018 - June 2019

#ifndef REPORT_H
#define REPORT_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OUT  0x00
#define LOG1 0x01
#define LOG2 0x02
#define TRC0 0x10
#define WRN0 0x20
#define ERR0 0x30
// Upper two bits of each nybble reserved: 
// id is --xx--yy where xx= category bits yy=level bits
#define REPORT_CID_MASK   0x30
#define REPORT_CID_SHIFT  4
#define REPORT_LID_MASK   0x03
#define REPORT_LID_SHIFT  0

/***/

extern int report (U8 id, const char fmt[], ...);
 
extern int reportN (U8 id, const char *a[], int n, const char start[], const char sep[], const char *end);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // REPORT_H
