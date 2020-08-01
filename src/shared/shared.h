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

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cinttypes>

#include <array>
#include <string>

#include "shared/platform.h"

// ABI compat only, don't use
typedef enum
{
	qfalse,
	qtrue
} qboolean;

/*
==============================================================

TIME

==============================================================
*/

// High resolution timer
uint64_t Q_time();

/*
==============================================================

MATHLIB

==============================================================
*/

using vec_t = float;

typedef struct
{
	float	x, y, z;
} vec3_t;

inline vec_t DotProduct(const vec3_t l, const vec3_t r)
{
	return l.x * r.x + l.y * r.y + l.z * r.z;
}

inline vec3_t VectorAdd(const vec3_t l, const vec3_t r)
{
	return {
		l.x + r.x,
		l.y + r.y,
		l.z + r.z
	};
}

inline vec3_t VectorSubtract(const vec3_t l, const vec3_t r)
{
	return {
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
	return {
		v.x * r,
		v.y * r,
		v.z * r 
	};
}

inline vec3_t VectorScaleI(const vec3_t v, const int32_t r)
{
	return {
		v.x * r,
		v.y * r,
		v.z * r 
	};
}

inline vec3_t VectorDivideF(const vec3_t v, const vec_t r)
{
	return {
		v.x / r,
		v.y / r,
		v.z / r 
	};
}

inline vec3_t VectorDivideI(const vec3_t v, const int32_t r)
{
	return {
		v.x / r,
		v.y / r,
		v.z / r 
	};
}

/*
==============================================================

MATH

==============================================================
*/

constexpr float coord2short = 8.f;
constexpr float angle2short = (65536.f / 360.f);
constexpr float short2coord = (1.0f / 8);
constexpr float short2angle = (360.0f / 65536);

template<typename T>
constexpr T bit(const T &shift)
{
	return 1 << shift;
}

template<typename T>
constexpr T max(const T &a, const T &b)
{
	return a > b ? a : b;
}

template<typename T>
constexpr T min(const T &a, const T &b)
{
	return a < b ? a : b;
}

/*
==============================================================

STRING

==============================================================
*/

constexpr size_t MAX_QPATH	= 64;	// max length of a quake game pathname

// buffer safe operations
size_t Q_strlcpy(char *dst, const char *src, size_t size);

char *va(const char *format, ...) q_printf(1, 2);

template<typename ...T>
inline std::string vas(const char *format, T ...args)
{
	static char ts;
	const size_t len = 1 + snprintf(&ts, 1, format, args...);
	std::string s;
	s.resize(len);
	snprintf(s.data(), len, format, args...);
	s.pop_back();
	return s;
}

template<>
inline std::string vas(const char *data)
{
	return data;
}

constexpr size_t MAX_INFO_STRING	= 512;

/*
==========================================================

	ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#include "api.h"
#include "client.h"
#include "entity.h"
