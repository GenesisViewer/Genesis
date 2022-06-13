/*
 * Copyright (c) 2002-2007, Communications and Remote Sensing Laboratory, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2007, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux and Antonin Descampe
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

#define OPJ_SKIP_POISON
#include "opj_includes.h"

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#if defined(__GNUC__)
#pragma GCC poison malloc calloc realloc free
#endif

/* <summary> */
/* This table contains the norms of the basis function of the reversible MCT. */
/* </summary> */
static const double mct_norms[3] = { 1.732, .8292, .8292 };

/* <summary> */
/* This table contains the norms of the basis function of the irreversible MCT. */
/* </summary> */
static const double mct_norms_real[3] = { 1.732, 1.805, 1.573 };

/* <summary> */
/* Foward reversible MCT. */
/* </summary> */
void mct_encode(
		int* OPJ_RESTRICT c0,
		int* OPJ_RESTRICT c1,
		int* OPJ_RESTRICT c2,
		int n)
{
	int i = 0;
#ifdef __SSE2__
	/* Buffers are normally aligned on 16 bytes... */
	if (((size_t)c0 & 0xf) == 0 && ((size_t)c1 & 0xf) == 0 && ((size_t)c2 & 0xf) == 0) {
		const int cnt = n & ~3U;
		for (; i < cnt; i += 4) {
			__m128i y, u, v;
			__m128i r = _mm_load_si128((const __m128i*) & (c0[i]));
			__m128i g = _mm_load_si128((const __m128i*) & (c1[i]));
			__m128i b = _mm_load_si128((const __m128i*) & (c2[i]));
			y = _mm_add_epi32(g, g);
			y = _mm_add_epi32(y, b);
			y = _mm_add_epi32(y, r);
			y = _mm_srai_epi32(y, 2);
			u = _mm_sub_epi32(b, g);
			v = _mm_sub_epi32(r, g);
			_mm_store_si128((__m128i*) & (c0[i]), y);
			_mm_store_si128((__m128i*) & (c1[i]), u);
			_mm_store_si128((__m128i*) & (c2[i]), v);
		}
	}
#endif
	for (; i < n; ++i) {
		int r = c0[i];
		int g = c1[i];
		int b = c2[i];
		int y = (r + g + g + b) >> 2;
		int u = b - g;
		int v = r - g;
		c0[i] = y;
		c1[i] = u;
		c2[i] = v;
	}
}

/* <summary> */
/* Inverse reversible MCT. */
/* </summary> */
void mct_decode(
		int* OPJ_RESTRICT c0,
		int* OPJ_RESTRICT c1,
		int* OPJ_RESTRICT c2,
		int n)
{
	int i = 0;
#ifdef __SSE2__
	/* Buffers are normally aligned on 16 bytes... */
	if (((size_t)c0 & 0xf) == 0 && ((size_t)c1 & 0xf) == 0 && ((size_t)c2 & 0xf) == 0) {
		const int cnt = n & ~3U;
		for (; i < cnt; i += 4) {
			__m128i r, g, b;
			__m128i y = _mm_load_si128((const __m128i*) & (c0[i]));
			__m128i u = _mm_load_si128((const __m128i*) & (c1[i]));
			__m128i v = _mm_load_si128((const __m128i*) & (c2[i]));
			g = y;
			g = _mm_sub_epi32(g, _mm_srai_epi32(_mm_add_epi32(u, v), 2));
			r = _mm_add_epi32(v, g);
			b = _mm_add_epi32(u, g);
			_mm_store_si128((__m128i*) & (c0[i]), r);
			_mm_store_si128((__m128i*) & (c1[i]), g);
			_mm_store_si128((__m128i*) & (c2[i]), b);
}
	}
#endif
	for (; i < n; ++i) {
		int y = c0[i];
		int u = c1[i];
		int v = c2[i];
		int g = y - ((u + v) >> 2);
		int r = v + g;
		int b = u + g;
		c0[i] = r;
		c1[i] = g;
		c2[i] = b;
	}
}

/* <summary> */
/* Get norm of basis function of reversible MCT. */
/* </summary> */
double mct_getnorm(int compno) {
	return mct_norms[compno];
}

/* <summary> */
/* Foward irreversible MCT. */
/* </summary> */
void mct_encode_real(
		int* OPJ_RESTRICT c0,
		int* OPJ_RESTRICT c1,
		int* OPJ_RESTRICT c2,
		int n)
{
	int i = 0;
#ifdef __SSE4_1__
	/* Buffers are normally aligned on 16 bytes... */
	if (((size_t)c0 & 0xf) == 0 && ((size_t)c1 & 0xf) == 0 && ((size_t)c2 & 0xf) == 0) {
		const int cnt = n & ~3U;
		const __m128i ry = _mm_set1_epi32(2449);
		const __m128i gy = _mm_set1_epi32(4809);
		const __m128i by = _mm_set1_epi32(934);
		const __m128i ru = _mm_set1_epi32(1382);
		const __m128i gu = _mm_set1_epi32(2714);
		const __m128i gv = _mm_set1_epi32(3430);
		const __m128i bv = _mm_set1_epi32(666);
		const __m128i mulround = _mm_shuffle_epi32(_mm_cvtsi32_si128(4096), _MM_SHUFFLE(1, 0, 1, 0));
		for (; i < cnt; i += 4) {
			__m128i lo, hi, y, u, v;
			__m128i r = _mm_load_si128((const __m128i*) & (c0[i]));
			__m128i g = _mm_load_si128((const __m128i*) & (c1[i]));
			__m128i b = _mm_load_si128((const __m128i*) & (c2[i]));

			hi = _mm_shuffle_epi32(r, _MM_SHUFFLE(3, 3, 1, 1));
			lo = _mm_mul_epi32(r, ry);
			hi = _mm_mul_epi32(hi, ry);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			y = _mm_blend_epi16(lo, hi, 0xCC);

			hi = _mm_shuffle_epi32(g, _MM_SHUFFLE(3, 3, 1, 1));
			lo = _mm_mul_epi32(g, gy);
			hi = _mm_mul_epi32(hi, gy);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			y = _mm_add_epi32(y, _mm_blend_epi16(lo, hi, 0xCC));

			hi = _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 3, 1, 1));
			lo = _mm_mul_epi32(b, by);
			hi = _mm_mul_epi32(hi, by);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			y = _mm_add_epi32(y, _mm_blend_epi16(lo, hi, 0xCC));
			_mm_store_si128((__m128i*) & (c0[i]), y);

			lo = _mm_cvtepi32_epi64(_mm_shuffle_epi32(b, _MM_SHUFFLE(3, 2, 2, 0)));
			hi = _mm_cvtepi32_epi64(_mm_shuffle_epi32(b, _MM_SHUFFLE(3, 2, 3, 1)));
			lo = _mm_slli_epi64(lo, 12);
			hi = _mm_slli_epi64(hi, 12);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			u = _mm_blend_epi16(lo, hi, 0xCC);

			hi = _mm_shuffle_epi32(r, _MM_SHUFFLE(3, 3, 1, 1));
			lo = _mm_mul_epi32(r, ru);
			hi = _mm_mul_epi32(hi, ru);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			u = _mm_sub_epi32(u, _mm_blend_epi16(lo, hi, 0xCC));

			hi = _mm_shuffle_epi32(g, _MM_SHUFFLE(3, 3, 1, 1));
			lo = _mm_mul_epi32(g, gu);
			hi = _mm_mul_epi32(hi, gu);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			u = _mm_sub_epi32(u, _mm_blend_epi16(lo, hi, 0xCC));
			_mm_store_si128((__m128i*) & (c1[i]), u);

			lo = _mm_cvtepi32_epi64(_mm_shuffle_epi32(r, _MM_SHUFFLE(3, 2, 2, 0)));
			hi = _mm_cvtepi32_epi64(_mm_shuffle_epi32(r, _MM_SHUFFLE(3, 2, 3, 1)));
			lo = _mm_slli_epi64(lo, 12);
			hi = _mm_slli_epi64(hi, 12);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			v = _mm_blend_epi16(lo, hi, 0xCC);

			hi = _mm_shuffle_epi32(g, _MM_SHUFFLE(3, 3, 1, 1));
			lo = _mm_mul_epi32(g, gv);
			hi = _mm_mul_epi32(hi, gv);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			v = _mm_sub_epi32(v, _mm_blend_epi16(lo, hi, 0xCC));

			hi = _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 3, 1, 1));
			lo = _mm_mul_epi32(b, bv);
			hi = _mm_mul_epi32(hi, bv);
			lo = _mm_add_epi64(lo, mulround);
			hi = _mm_add_epi64(hi, mulround);
			lo = _mm_srli_epi64(lo, 13);
			hi = _mm_slli_epi64(hi, 32 - 13);
			v = _mm_sub_epi32(v, _mm_blend_epi16(lo, hi, 0xCC));
			_mm_store_si128((__m128i*) & (c2[i]), v);
		}
	}
#endif
	for (; i < n; ++i) {
		int r = c0[i];
		int g = c1[i];
		int b = c2[i];
		int y =  fix_mul(r, 2449) + fix_mul(g, 4809) + fix_mul(b, 934);
		int u = -fix_mul(r, 1382) - fix_mul(g, 2714) + fix_mul(b, 4096);
		int v =  fix_mul(r, 4096) - fix_mul(g, 3430) - fix_mul(b, 666);
		c0[i] = y;
		c1[i] = u;
		c2[i] = v;
	}
}

/* <summary> */
/* Inverse irreversible MCT. */
/* </summary> */
void mct_decode_real(
		float* OPJ_RESTRICT c0,
		float* OPJ_RESTRICT c1,
		float* OPJ_RESTRICT c2,
		int n)
{
	int i;
#ifdef __SSE__
	int count;
	__m128 vrv, vgu, vgv, vbu;
	vrv = _mm_set1_ps(1.402f);
	vgu = _mm_set1_ps(0.34413f);
	vgv = _mm_set1_ps(0.71414f);
	vbu = _mm_set1_ps(1.772f);
	count = n >> 3;
	for (i = 0; i < count; ++i) {
		__m128 vy, vu, vv;
		__m128 vr, vg, vb;

		vy = _mm_load_ps(c0);
		vu = _mm_load_ps(c1);
		vv = _mm_load_ps(c2);
		vr = _mm_add_ps(vy, _mm_mul_ps(vv, vrv));
		vg = _mm_sub_ps(_mm_sub_ps(vy, _mm_mul_ps(vu, vgu)), _mm_mul_ps(vv, vgv));
		vb = _mm_add_ps(vy, _mm_mul_ps(vu, vbu));
		_mm_store_ps(c0, vr);
		_mm_store_ps(c1, vg);
		_mm_store_ps(c2, vb);
		c0 += 4;
		c1 += 4;
		c2 += 4;

		vy = _mm_load_ps(c0);
		vu = _mm_load_ps(c1);
		vv = _mm_load_ps(c2);
		vr = _mm_add_ps(vy, _mm_mul_ps(vv, vrv));
		vg = _mm_sub_ps(_mm_sub_ps(vy, _mm_mul_ps(vu, vgu)), _mm_mul_ps(vv, vgv));
		vb = _mm_add_ps(vy, _mm_mul_ps(vu, vbu));
		_mm_store_ps(c0, vr);
		_mm_store_ps(c1, vg);
		_mm_store_ps(c2, vb);
		c0 += 4;
		c1 += 4;
		c2 += 4;
	}
	n &= 7;
#endif
	for(i = 0; i < n; ++i) {
		float y = c0[i];
		float u = c1[i];
		float v = c2[i];
		float r = y + (v * 1.402f);
		float g = y - (u * 0.34413f) - (v * 0.71414f);
		float b = y + (u * 1.772f);
		c0[i] = r;
		c1[i] = g;
		c2[i] = b;
	}
}

/* <summary> */
/* Get norm of basis function of irreversible MCT. */
/* </summary> */
double mct_getnorm_real(int compno) {
	return mct_norms_real[compno];
}
