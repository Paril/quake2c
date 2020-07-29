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

struct vec3_t : std::array<vec_t, 3>
{
	constexpr vec_t operator*(const vec3_t &r) const
	{
		return this->at(0) * r.at(0) + this->at(1) * r.at(1) + this->at(2) * r.at(2);
	}

	constexpr vec3_t operator+(const vec3_t &r) const
	{
		return {
			this->at(0) + r.at(0),
			this->at(1) + r.at(1),
			this->at(2) + r.at(2)
		};
	}

	constexpr vec3_t operator-(const vec3_t &r) const
	{
		return {
			this->at(0) - r.at(0),
			this->at(1) - r.at(1),
			this->at(2) - r.at(2)
		};
	}

	constexpr bool operator!() const
	{
		return !this->at(0) && !this->at(1) && !this->at(2);
	}

	template<typename TR>
	constexpr vec3_t operator*(const TR &r) const
	{
		return {
			this->at(0) * r,
			this->at(1) * r,
			this->at(2) * r 
		};
	}

	template<typename TR>
	constexpr vec3_t operator/(const TR &r) const
	{
		return {
			this->at(0) / r,
			this->at(1) / r,
			this->at(2) / r 
		};
	}

	constexpr vec3_t &operator*=(const vec_t &r)
	{
		*this = *this * r;
		return *this;
	}

	constexpr vec3_t &operator/=(const vec_t &r)
	{
		*this = *this / r;
		return *this;
	}

	constexpr vec3_t &operator+=(const vec3_t &r)
	{
		*this = *this + r;
		return *this;
	}

	constexpr vec3_t &operator-=(const vec3_t &r)
	{
		*this = *this - r;
		return *this;
	}
};

template<typename TR>
constexpr vec3_t operator*(const TR &l, const vec3_t &r)
{
	return r * l;
}

constexpr vec_t DotProduct(const vec3_t &x, const vec3_t &y)
{
	return x * y;
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

inline std::string vtoss(const vec3_t v)
{
	return vas("%f %f %f", v[0], v[1], v[2]);
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
