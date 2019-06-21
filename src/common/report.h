// report.h - debug/log/error report filtering
// https://github.com/DrAl-HFS/*.git
// (c) Project Contributors Jan-June 2019

#ifndef REPORT_H
#define REPORT_H

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// Specials: NB supercedes level 0xF FRCD|WRN1 and FRCD|ERR1 ids
#define OUT  0xFF // -> stdout as is: no prefix, no masking
#define DBG  0xFE // -> stderr   "     "           "

// Basic report categories
#define LOG0 0x0 // general log -> stderr   : no prefix, maskable
#define TRC0 0x1 // call trace -> stderr   : prefixed,    "
#define WRN0 0x2 // warning         "           "         "
#define ERR0 0x3 // error           "           "         "
// Flags
#define NDNT (1<<2) // Indent (without identifying prefix)
#define FRCD (1<<3) // Force: disable masking per instance

// Define compact values for continuation reports (indented without prefix)
#define LOG1 (NDNT|LOG0)
#define TRC1 (NDNT|TRC0)
#define WRN1 (NDNT|WRN0)
#define ERR1 (NDNT|ERR0)

// Upper nybble gives user defined level for flexible masking
// bits7..0 = llllffcc : llll=level, ff=flags, cc=category
#define REPORT_FCID_MASK   0x0F
#define REPORT_FCID_SHIFT  0
#define REPORT_LNID_MASK   0xF0
#define REPORT_LNID_SHIFT  4

// Convenience macros (can be locally/globally undefined)
#define REPORT(id, fmt, ...) report(id, fmt, __VA_ARGS__)
#define LOG(fmt, ...)   report(LOG0, fmt, __VA_ARGS__)
#define TRACE(fmt, ...) report(TRC0, fmt, __VA_ARGS__)
#define WARN(fmt, ...)  report(WRN0, fmt, __VA_ARGS__)
#define ERROR(fmt, ...) report(ERR0, fmt, __VA_ARGS__)
// function call monitoring
// The string fmt allows showing actual arg values and result e.g. "(%p,%u) -> %d"
// NB: adjacent string concatenation, compiler defined symbol, variadic args
#define REPORT_CALL(id, fmt, ...)  report(id, "%s"fmt, __func__, __VA_ARGS__)
#define LOG_CALL(fmt, ...)     report(LOG0, "%s"fmt, __func__, __VA_ARGS__)
#define TRACE_CALL(fmt, ...)   report(TRC0, "%s"fmt, __func__, __VA_ARGS__)
#define WARN_CALL(fmt, ...)    report(WRN0, "%s"fmt, __func__, __VA_ARGS__)
#define ERROR_CALL(fmt, ...)   report(ERR0, "%s"fmt, __func__, __VA_ARGS__)


/***/

// General filtered reporting
extern int report (const U8 id, const char fmt[], ...);

// Buffer hex dump
extern int reportBytes (const U8 id, const U8 *pB, int nB);

// Hacky multi-string assembly reporting: only goes to OUT or DBG for now
extern int reportN (const U8 id, const char *a[], int n, const char start[], const char sep[], const char *end);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // REPORT_H
