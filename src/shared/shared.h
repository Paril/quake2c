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
enum qboolean
{
	qfalse,
	qtrue
};

/*
==============================================================

MATHLIB

==============================================================
*/

// angle indexes
enum : size_t
{
	PITCH,	// up / down
	YAW,	// left / right
	ROLL	// fall over
};

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

constexpr vec3_t vec3_origin { 0, 0, 0 };

constexpr vec_t DEG2RAD(const vec_t &a)
{
	return a * (M_PI / 180);
}

constexpr vec_t RAD2DEG(const vec_t &a)
{
	return a * (180 / M_PI);
}

constexpr vec_t DotProduct(const vec3_t &x, const vec3_t &y)
{
	return x * y;
}

constexpr void CrossProduct(const vec3_t &v1, const vec3_t &v2, vec3_t &cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

constexpr vec_t VectorLengthSquared(const vec3_t &v)
{
	return DotProduct(v, v);
}

inline vec_t VectorLength(const vec3_t &v)
{
	return sqrtf(VectorLengthSquared(v));
}

inline void AngleVectors(const vec3_t &angles, vec3_t &forward, vec3_t &right, vec3_t &up)
{
    float        angle;
    float        sr, sp, sy, cr, cp, cy;

    angle = DEG2RAD(angles[YAW]);
    sy = sin(angle);
    cy = cos(angle);
    angle = DEG2RAD(angles[PITCH]);
    sp = sin(angle);
    cp = cos(angle);
    angle = DEG2RAD(angles[ROLL]);
    sr = sin(angle);
    cr = cos(angle);

	forward = { cp * cy, cp * sy, -sp };
    right = {
		(-1 * sr * sp * cy + -1 * cr * -sy),
		(-1 * sr * sp * sy + -1 * cr * cy),
		-1 * sr * cp
	};
	up = {
		(cr * sp * cy + -sr * -sy),
		(cr * sp * sy + -sr * cy),
		cr * cp
	};
}

inline vec_t VectorNormalize(vec3_t &v)
{
	vec_t length = VectorLength(v);

	if (length)
	{
		vec_t ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

/*
==============================================================

RANDOM

==============================================================
*/

void Q_srand(uint32_t seed);
uint32_t Q_rand();
uint32_t Q_rand_uniform(uint32_t n);
float frand();
float crand();

/*
==============================================================

MATH

==============================================================
*/

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

constexpr int16_t ANGLE2SHORT(const vec_t &x)
{
	return static_cast<int32_t>(x * 65536 / 360) & 65535;
}

constexpr vec_t SHORT2ANGLE(const int16_t &x)
{
	return x * 360.0f / 65536;
}

constexpr int16_t COORD2SHORT(const vec_t &x)
{
	return x * 8.0f;
}

constexpr vec_t SHORT2COORD(const int16_t &x)
{
	return x * (1.0f / 8);
}

/*
==============================================================

STRING

==============================================================
*/

constexpr size_t MAX_TOKEN_CHARS	= 1024;	// max length of an individual token
constexpr size_t MAX_QPATH	= 64;	// max length of a quake game pathname

#define Q_stricmp   stricmp
#define Q_stricmpn  strnicmp

char *COM_Parse(const char **data_p);

// buffer safe operations
size_t Q_strlcpy(char *dst, const char *src, size_t size);

size_t Q_vsnprintf(char *dest, size_t size, const char *fmt, va_list argptr);
size_t Q_snprintf(char *dest, size_t size, const char *fmt, ...) q_printf(3, 4);

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

//
// key / value info strings
//
constexpr size_t MAX_INFO_KEY		= 64;
constexpr size_t MAX_INFO_VALUE		= 64;
constexpr size_t MAX_INFO_STRING	= 512;

char    *Info_ValueForKey(const char *s, const char *key);
void    Info_RemoveKey(char *s, const char *key);
bool    Info_SetValueForKey(char *s, const char *key, const char *value);
bool    Info_Validate(const char *s);

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#include "api.h"
#include "client.h"
#include "entity.h"

template <typename T, class... StreamArgs>
inline std::basic_ostream<StreamArgs...> &
operator <= (std::basic_ostream<StreamArgs...> & out, T const & data) {
        out.write(reinterpret_cast<char const *>(&data), sizeof(T));
        return out;
}

template <typename T, class... StreamArgs>
inline std::basic_istream<StreamArgs...> &
operator >= (std::basic_istream<StreamArgs...> & out, T & data) {
        out.read(reinterpret_cast<char *>(&data), sizeof(T));
        return out;
}