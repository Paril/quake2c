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