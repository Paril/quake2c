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
===============
Q_strlcpy

Returns length of the source string.
===============
*/
size_t Q_strlcpy(char *dst, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = size - 1;
		
		if (ret < len)
			len = ret;

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
    return a & (hash_size - 1);
}

/*
================
Q_next_pow2
================
*/
uint64_t Q_next_pow2(uint64_t x)
{
#ifdef __GNU__
	return x == 1 ? 1 : 1 << (64 - __builtin_clzl((uint32_t)(x - 1)));
#else
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return x + 1;
#endif
}