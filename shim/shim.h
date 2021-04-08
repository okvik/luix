#ifndef _SHIM_H_
#define _SHIM_H_

/** PLAN 9 **/
#ifndef nil
#include <u.h>
#include <libc.h>
#include <tos.h>
#endif

/** assert.h **/

/** ctype.h **/

/** errno.h **/
typedef int errno_t;
extern errno_t *priv_errno;
#define errno (*priv_errno)

/** float.h **/
#define FLT_ROUNDS	1
#define FLT_RADIX	2

#define FLT_DIG		6
#define FLT_EPSILON	1.19209290e-07
#define FLT_MANT_DIG	24
#define FLT_MAX		3.40282347e+38
#define FLT_MAX_10_EXP	38
#define FLT_MAX_EXP	128
#define FLT_MIN		1.17549435e-38
#define FLT_MIN_10_EXP	-37
#define FLT_MIN_EXP	-125

#define DBL_DIG		15
#define DBL_EPSILON	2.2204460492503131e-16
#define DBL_MANT_DIG	53
#define DBL_MAX		1.797693134862315708145e+308
#define DBL_MAX_10_EXP	308
#define DBL_MAX_EXP	1024
#define DBL_MIN		2.225073858507201383090233e-308
#define DBL_MIN_10_EXP	-307
#define DBL_MIN_EXP	-1021
#define LDBL_MANT_DIG	DBL_MANT_DIG
#define LDBL_EPSILON	DBL_EPSILON
#define LDBL_DIG	DBL_DIG
#define LDBL_MIN_EXP	DBL_MIN_EXP
#define LDBL_MIN	DBL_MIN
#define LDBL_MIN_10_EXP	DBL_MIN_10_EXP
#define LDBL_MAX_EXP	DBL_MAX_EXP
#define LDBL_MAX	DBL_MAX
#define LDBL_MAX_10_EXP	DBL_MAX_10_EXP

/** limits.h **/
#define CHAR_BIT	8
#define MB_LEN_MAX	4

#define UCHAR_MAX	0xff
#define USHRT_MAX	0xffff
#define UINT_MAX	0xffffffffU
#define ULONG_MAX	0xffffffffUL
#define ULLONG_MAX	0xffffffffffffffffULL

#define CHAR_MAX	SCHAR_MAX
#define SCHAR_MAX	0x7f
#define SHRT_MAX	0x7fff
#define INT_MAX		0x7fffffff
#define LONG_MAX	0x7fffffffL
#define LLONG_MAX	0x7fffffffffffffffLL

#define CHAR_MIN	SCHAR_MIN
#define SCHAR_MIN	(-SCHAR_MAX-1)
#define SHRT_MIN	(-SHRT_MAX-1)
#define INT_MIN		(-INT_MAX-1)
#define LONG_MIN	(-LONG_MAX-1)
#define LLONG_MIN	(-LLONG_MAX-1)

/** locale.h **/

/** math.h **/
#define HUGE_VAL Inf(1)

/** regex.h **/

/** setjmp.h **/

/** signal.h **/
typedef int sig_atomic_t;

/** stdarg.h **/

/** stdbool.h **/

/** stddef.h **/
#define NULL ((void*)0)

typedef long long ptrdiff_t;
typedef unsigned long long size_t;

/** stdio.h **/
#include "/sys/include/stdio.h"

/** stdlib.h **/

/** string.h **/
char *strerror(int);

#define strcoll strcmp

/** time.h **/
#define CLOCKS_PER_SEC 1000

typedef long clock_t;
typedef long time_t;

clock_t clock(void);
#define clock() (clock_t)(-1);

#endif /* _SHIM_H_ */
