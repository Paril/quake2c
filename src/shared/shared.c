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

#include "shared/shared.h"

// From https://gist.github.com/ForeverZer0/0a4f80fc02b96e19380ebb7a3debbee5
#include <stdint.h>
#if defined(__linux)
#  define HAVE_POSIX_TIMER
#  include <time.h>
#  ifdef CLOCK_MONOTONIC
#     define CLOCKID CLOCK_MONOTONIC
#  else
#     define CLOCKID CLOCK_REALTIME
#  endif
#elif defined(__APPLE__)
#  define HAVE_MACH_TIMER
#  include <mach/mach_time.h>
static mach_timebase_info_data_t info;
#elif defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define _WIN32_WINNT 0x0601
#  include <Windows.h>
static LARGE_INTEGER win_frequency;
#endif
uint64_t Q_time(void)
{
	static bool is_init = false;
#if defined(__APPLE__)
	if (!is_init)
	{
		mach_timebase_info(&info);
		is_init = true;
	}
	uint64_t now;
	now = mach_absolute_time();
	return now;
#elif defined(__linux)
	static struct timespec linux_rate;
	if (!is_init)
	{
		clock_getres(CLOCKID, &linux_rate);
		is_init = true;
	}
	uint64_t now;
	struct timespec spec;
	clock_gettime(CLOCKID, &spec);
	now = spec.tv_sec * 1.0e9 + spec.tv_nsec;
	return now;
#elif defined(_WIN32)
	if (!is_init)
	{
		QueryPerformanceFrequency(&win_frequency);
		is_init = true;
	}
	static LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	//return (uint64_t) ((1e9 * now.QuadPart) / win_frequency.QuadPart);
	return now.QuadPart;
#endif
}

uint64_t Q_time_adjust(const uint64_t time)
{
#if defined(__APPLE__)
	return (time * info.numer) / info.denom;
#elif defined(__linux)
	return time;
#elif defined(_WIN32)
	return (uint64_t) ((1e9 * time) / win_frequency.QuadPart);
#endif
}

/*
===============
Q_strlcpy

Returns length of the source string.
===============
*/
size_t Q_strlcpy(char *dst, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = min(ret, size - 1);
		memcpy(dst, src, len);
		dst[len] = 0;
	}

	return ret;
}

/*
================
Q_hash_string
================
*/
uint32_t Q_hash_string (const char *string, const size_t hash_size)
{
	if (!hash_size)
		return 0;

	uint32_t	hashValue = 0;

	for (size_t i = 0; *string; i++)
	{
		const char ch = *(string++);
		hashValue = hashValue * 33 + ch;
	}

	return (hashValue + (hashValue >> 5)) % hash_size;
}

/*
================
Q_hash_pointer
================
*/
uint32_t Q_hash_pointer(uint32_t a, const size_t hash_size)
{
	if (!hash_size)
		return 0;

    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a % hash_size;
}
