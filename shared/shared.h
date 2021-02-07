/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

//
// shared.h -- included first by ALL program modules
//

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "shared/platform.h"

#if defined(__clang__) || defined(__GNUC__)
#define qcvm_always_inline __attribute__((always_inline))
#define qcvm_noreturn __attribute__((noreturn))
#elif defined(_MSC_VER)
#define qcvm_always_inline __forceinline
#ifndef __cplusplus
#define inline __inline
#endif
#define qcvm_noreturn
#endif

// ABI compat only, don't use
typedef enum
{
	qfalse,
	qtrue
} qboolean;

/*
==============================================================

MATHLIB

==============================================================
*/

typedef float vec_t;

typedef struct
{
	vec_t	x, y, z;
} vec3_t;

inline vec_t DotProduct(const vec3_t l, const vec3_t r)
{
	return l.x * r.x + l.y * r.y + l.z * r.z;
}

#ifdef __cplusplus
#define VEC3
#else
#define VEC3 (vec3_t)
#endif

inline vec3_t VectorAdd(const vec3_t l, const vec3_t r)
{
	return VEC3 {
		l.x + r.x,
		l.y + r.y,
		l.z + r.z
	};
}

inline vec3_t VectorSubtract(const vec3_t l, const vec3_t r)
{
	return VEC3 {
		l.x - r.x,
		l.y - r.y,
		l.z - r.z
	};
}

inline bool VectorEmpty(const vec3_t v)
{
	return !v.x && !v.y && !v.z;
}

inline bool VectorEquals(const vec3_t a, const vec3_t b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline vec3_t VectorScaleF(const vec3_t v, const vec_t r)
{
	return VEC3 {
		v.x * r,
		v.y * r,
		v.z * r 
	};
}

inline vec3_t VectorScaleI(const vec3_t v, const int32_t r)
{
	return VEC3 {
		v.x * (vec_t) r,
		v.y * (vec_t) r,
		v.z * (vec_t) r 
	};
}

inline vec3_t VectorDivideF(const vec3_t v, const vec_t r)
{
	return VEC3 {
		v.x / r,
		v.y / r,
		v.z / r 
	};
}

inline vec3_t VectorDivideI(const vec3_t v, const int32_t r)
{
	return VEC3 {
		v.x / (vec_t) r,
		v.y / (vec_t) r,
		v.z / (vec_t) r 
	};
}

/*
==============================================================

MATH

==============================================================
*/

#define coord2short (8.f)
#define angle2short (65536.f / 360.f)
#define short2coord (1.0f / 8)
#define short2angle (360.0f / 65536)

inline vec_t maxf(const vec_t a, const vec_t b) { return a > b ? a : b; }
inline vec_t minf(const vec_t a, const vec_t b) { return a < b ? a : b; }

inline int32_t maxi(const int32_t a, const int32_t b) { return a > b ? a : b; }
inline int32_t mini(const int32_t a, const int32_t b) { return a < b ? a : b; }

inline size_t maxsz(const size_t a, const size_t b) { return a > b ? a : b; }
inline size_t minsz(const size_t a, const size_t b) { return a < b ? a : b; }

/*
==============================================================

STRING

==============================================================
*/

enum { MAX_QPATH	= 64 };	// max length of a quake game pathname

// buffer safe operations
size_t Q_strlcpy(char *dst, const char *src, size_t size);

enum { MAX_INFO_STRING	= 512 };

uint32_t Q_hash_string(const char *string, const size_t hash_size);

uint32_t Q_hash_pointer(uint32_t a, const size_t hash_size);

uint64_t Q_next_pow2(uint64_t x);

/*
==========================================================

	ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#include "api.h"
#include "client.h"
#include "entity.h"
