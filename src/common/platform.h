// platform.h - fundamental type & macro declarations (to simplify multi-platform support)
// https://github.com/DrAl-HFS/*.git
// (c) Project Contributors Jan-June 2019

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// General compiler tweaks
//pragma clang diagnostic ignored "-Wmissing-field-initializers"

#ifndef SWAP
#define SWAP(Type,a,b) { Type tmp= (a); (a)= (b); (b)= tmp; }
#endif
#ifndef MIN
#define MIN(a,b) (a)<(b)?(a):(b)
#endif
#ifndef MAX
#define MAX(a,b) (a)>(b)?(a):(b)
#endif
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

//define _PASTE(a,b) a##b
//define PASTE(a,b) _PASTE(a,b)

// Terse type names
typedef signed char       I8;
typedef signed short      I16;
typedef signed int        I32;
// Beware! sizeof(long)==8 under PGI!
typedef signed long long I64;

typedef unsigned char      U8;
typedef unsigned short     U16;
typedef unsigned int       U32;
typedef unsigned long long U64;

typedef float F32;
typedef double F64;

typedef int Bool32;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PLATFORM_H
