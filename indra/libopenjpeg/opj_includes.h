/*
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef OPJ_INCLUDES_H
#define OPJ_INCLUDES_H

/*
 ==========================================================
   Standard includes used by the library
 ==========================================================
*/
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>

/*
 ==========================================================
   OpenJPEG interface
 ==========================================================
 */
#include "openjpeg.h"

/*
 ==========================================================
   OpenJPEG modules
 ==========================================================
*/

/* Are restricted pointers available? (C99) */
#if (__STDC_VERSION__ >= 199901L)
#define OPJ_RESTRICT restrict
#else
/* Not a C99 compiler */
#if defined(__GNUC__)
#define OPJ_RESTRICT __restrict__
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
#define OPJ_RESTRICT __restrict
#else
#define OPJ_RESTRICT /* restrict */
#endif
#endif

/* Ignore GCC attributes if this is not GCC */
#ifndef __GNUC__
	#define __attribute__(x) /* __attribute__(x) */
#endif


/* MSVC before 2013 and Borland C do not have lrintf */
#if defined(_MSC_VER)
#include <intrin.h>
static INLINE long opj_lrintf(float f)
{
#ifdef _M_X64
    return _mm_cvt_ss2si(_mm_load_ss(&f));

    /* commented out line breaks many tests */
    /* return (long)((f>0.0f) ? (f + 0.5f):(f -0.5f)); */
#elif defined(_M_IX86)
    int i;
    _asm{
        fld f
        fistp i
    };

    return i;
#else
    return (long)((f>0.0f) ? (f + 0.5f) : (f - 0.5f));
#endif
}
#elif defined(__BORLANDC__)
static INLINE long opj_lrintf(float f)
{
#ifdef _M_X64
    return (long)((f > 0.0f) ? (f + 0.5f) : (f - 0.5f));
#else
    int i;

    _asm {
        fld f
        fistp i
    };

    return i;
#endif
}
#else
static INLINE long opj_lrintf(float f)
{
    return lrintf(f);
}
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1400)
#define vsnprintf _vsnprintf
#endif

/* MSVC x86 is really bad at doing int64 = int32 * int32 on its own. Use intrinsic. */
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(__INTEL_COMPILER) && defined(_M_IX86)
#   include <intrin.h>
#   pragma intrinsic(__emul)
#endif

/* Apparently Visual Studio doesn't define __SSE__ / __SSE2__ macros */
#if defined(_M_X64)
/* Intel 64bit support SSE and SSE2 */
#   ifndef __SSE__
#       define __SSE__ 1
#   endif
#   ifndef __SSE2__
#       define __SSE2__ 1
#   endif
#   if !defined(__SSE4_1__) && defined(__AVX__)
#       define __SSE4_1__ 1
#   endif
#endif

/* For x86, test the value of the _M_IX86_FP macro. */
/* See https://msdn.microsoft.com/en-us/library/b0084kay.aspx */
#if defined(_M_IX86_FP)
#   if _M_IX86_FP >= 1
#       ifndef __SSE__
#           define __SSE__ 1
#       endif
#   endif
#   if _M_IX86_FP >= 2
#       ifndef __SSE2__
#           define __SSE2__ 1
#       endif
#   endif
#endif

/* Type to use for bit-fields in internal headers */
typedef unsigned int OPJ_BITFIELD;

#define OPJ_UNUSED(x) (void)x

#include "j2k_lib.h"
#include "opj_malloc.h"
#include "event.h"
#include "bio.h"
#include "cio.h"

#include "image.h"
#include "j2k.h"
#include "jp2.h"
#include "jpt.h"

#include "mqc.h"
#include "raw.h"
#include "bio.h"
#include "tgt.h"
#include "pi.h"
#include "tcd.h"
#include "t1.h"
#include "dwt.h"
#include "t2.h"
#include "mct.h"
#include "int.h"
#include "fix.h"

#include "cidx_manager.h"
#include "indexbox_manager.h"

/* JPWL>> */
#ifdef USE_JPWL
#include "./jpwl/jpwl.h"
#endif /* USE_JPWL */
/* <<JPWL */

#endif /* OPJ_INCLUDES_H */
