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
#elif defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#endif
uint64_t Q_time() {
  static bool is_init = false;
#if defined(__APPLE__)
	static mach_timebase_info_data_t info;
	if (!is_init)
	{
		mach_timebase_info(&info);
		is_init = true;
	}
	uint64_t now;
	now = mach_absolute_time();
	now *= info.numer;
	now /= info.denom;
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
	static LARGE_INTEGER win_frequency;
	if (!is_init)
	{
		QueryPerformanceFrequency(&win_frequency);
		is_init = true;
	}
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (uint64_t) ((1e9 * now.QuadPart)  / win_frequency.QuadPart);
#endif
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *va(const char *format, ...)
{
	va_list         argptr;
	static std::array<char[0x2800], 8>     buffers;
	static int      index;

	index = (index + 1) % buffers.size();

	va_start(argptr, format);
	vsnprintf(buffers[index], sizeof(buffers[0]), format, argptr);
	va_end(argptr);

	return buffers[index];
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