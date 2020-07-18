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
	Q_vsnprintf(buffers[index], sizeof(buffers[0]), format, argptr);
	va_end(argptr);

	return buffers[index];
}

static char     com_token[4][MAX_TOKEN_CHARS];
static int      com_tokidx;

/*
==============
COM_Parse

Parse a token out of a string.
Handles C and C++ comments.
==============
*/
char *COM_Parse(const char **data_p)
{
	int         c;
	int         len;
	const char  *data;
	char        *s = com_token[com_tokidx++ & 3];

	data = *data_p;
	len = 0;
	s[0] = 0;

	if (!data) {
		*data_p = NULL;
		return s;
	}

// skip whitespace
skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0) {
			*data_p = NULL;
			return s;
		}
		data++;
	}

// skip // comments
	if (c == '/' && data[1] == '/') {
		data += 2;
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// skip /* */ comments
	if (c == '/' && data[1] == '*') {
		data += 2;
		while (*data) {
			if (data[0] == '*' && data[1] == '/') {
				data += 2;
				break;
			}
			data++;
		}
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || !c) {
				goto finish;
			}

			if (len < MAX_TOKEN_CHARS - 1) {
				s[len++] = c;
			}
		}
	}

// parse a regular word
	do {
		if (len < MAX_TOKEN_CHARS - 1) {
			s[len++] = c;
		}
		data++;
		c = *data;
	} while (c > 32);

finish:
	s[len] = 0;

	*data_p = data;
	return s;
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

extern game_import_t gi;

/*
===============
Q_vsnprintf

Returns number of characters that would be written into the buffer,
excluding trailing '\0'. If the returned value is equal to or greater than
buffer size, resulting string is truncated.

WARNING: On Win32, until MinGW-w64 vsnprintf() bug is fixed, this may return
SIZE_MAX on overflow. Only use return value to test for overflow, don't use
it to allocate memory.
===============
*/
size_t Q_vsnprintf(char *dest, size_t size, const char *fmt, va_list argptr)
{
	int ret;

	if (size > INT_MAX)
		gi.error("%s: bad buffer size", __func__);

#ifdef _WIN32
	if (size) {
		ret = _vsnprintf(dest, size - 1, fmt, argptr);
		if (ret < 0 || ret >= size - 1)
			dest[size - 1] = 0;
	} else {
		ret = _vscprintf(fmt, argptr);
	}
#else
	ret = vsnprintf(dest, size, fmt, argptr);
#endif

	return ret;
}

/*
===============
Q_snprintf

Returns number of characters that would be written into the buffer,
excluding trailing '\0'. If the returned value is equal to or greater than
buffer size, resulting string is truncated.

WARNING: On Win32, until MinGW-w64 vsnprintf() bug is fixed, this may return
SIZE_MAX on overflow. Only use return value to test for overflow, don't use
it to allocate memory.
===============
*/
size_t Q_snprintf(char *dest, size_t size, const char *fmt, ...)
{
	va_list argptr;
	size_t  ret;

	va_start(argptr, fmt);
	ret = Q_vsnprintf(dest, size, fmt, argptr);
	va_end(argptr);

	return ret;
}

/*
===============
Q_memccpy

Copies no more than 'size' bytes stopping when 'c' character is found.
Returns pointer to next byte after 'c' in 'dst', or NULL if 'c' was not found.
===============
*/
void *Q_memccpy(void *dst, const void *src, int c, size_t size)
{
	uint8_t *d = reinterpret_cast<uint8_t *>(dst);
	const uint8_t *s = reinterpret_cast<const uint8_t *>(src);

	while (size--) {
		if ((*d++ = *s++) == c) {
			return d;
		}
	}

	return NULL;
}

void Q_setenv(const char *name, const char *value)
{
#ifdef _WIN32
	if (!value) {
		value = "";
	}
#if (_MSC_VER >= 1400)
	_putenv_s(name, value);
#else
	_putenv(va("%s=%s", name, value));
#endif
#else // _WIN32
	if (value) {
		setenv(name, value, 1);
	} else {
		unsetenv(name);
	}
#endif // !_WIN32
}

/*
=====================================================================

  MT19337 PRNG

=====================================================================
*/

#include <random>

static std::mt19937 mt;

/*
==================
Q_srand

Seed PRNG with initial value
==================
*/
void Q_srand(uint32_t seed)
{
	mt.seed(seed);
}

/*
==================
Q_rand

Generate random integer in range [0, 2^32)
==================
*/
uint32_t Q_rand(void)
{
	return mt();
}

/*
==================
Q_rand_uniform

Generate random integer in range [0, n) avoiding modulo bias
==================
*/
uint32_t Q_rand_uniform(uint32_t n)
{
	return std::uniform_int_distribution<uint32_t>(0, n - 1)(mt);
}

float frand()
{
	return std::uniform_real<float>()(mt);
}

float frand(const float &max)
{
	return std::uniform_real<float>(0.f, max)(mt);
}

float frand(const float &min, const float &max)
{
	return std::uniform_real<float>(min, max)(mt);
}


/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey(const char *s, const char *key)
{
	// use 4 buffers so compares work without stomping on each other
	static char value[4][MAX_INFO_STRING];
	static int  valueindex;
	char        pkey[MAX_INFO_STRING];
	char        *o;

	valueindex++;
	if (*s == '\\')
		s++;
	while (1) {
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				goto fail;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex & 3];
		while (*s != '\\' && *s) {
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey))
			return value[valueindex & 3];

		if (!*s)
			goto fail;
		s++;
	}

fail:
	o = value[valueindex & 3];
	*o = 0;
	return o;
}

/*
==================
Info_RemoveKey
==================
*/
void Info_RemoveKey(char *s, const char *key)
{
	char    *start;
	char    pkey[MAX_INFO_STRING];
	char    *o;

	while (1) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		while (*s != '\\' && *s) {
			s++;
		}

		if (!strcmp(key, pkey)) {
			o = start; // remove this part
			while (*s) {
				*o++ = *s++;
			}
			*o = 0;
			s = start;
			continue; // search for duplicates
		}

		if (!*s)
			return;
	}

}

/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing.
Also checks the length of keys/values and the whole string.
==================
*/
bool Info_Validate(const char *s)
{
	size_t len, total;
	int c;

	total = 0;
	while (1) {
		//
		// validate key
		//
		if (*s == '\\') {
			s++;
			if (++total == MAX_INFO_STRING) {
				return false;   // oversize infostring
			}
		}
		if (!*s) {
			return false;   // missing key
		}
		len = 0;
		while (*s != '\\') {
			c = *s++;
			if (!isprint(c) || c == '\"' || c == ';') {
				return false;   // illegal characters
			}
			if (++len == MAX_INFO_KEY) {
				return false;   // oversize key
			}
			if (++total == MAX_INFO_STRING) {
				return false;   // oversize infostring
			}
			if (!*s) {
				return false;   // missing value
			}
		}

		//
		// validate value
		//
		s++;
		if (++total == MAX_INFO_STRING) {
			return false;   // oversize infostring
		}
		if (!*s) {
			return false;   // missing value
		}
		len = 0;
		while (*s != '\\') {
			c = *s++;
			if (!isprint(c) || c == '\"' || c == ';') {
				return false;   // illegal characters
			}
			if (++len == MAX_INFO_VALUE) {
				return false;   // oversize value
			}
			if (++total == MAX_INFO_STRING) {
				return false;   // oversize infostring
			}
			if (!*s) {
				return true;    // end of string
			}
		}
	}
}

/*
============
Info_SubValidate
============
*/
size_t Info_SubValidate(const char *s)
{
	size_t len;
	int c;

	len = 0;
	while (*s) {
		c = *s++;
		c &= 127;       // strip high bits
		if (c == '\\' || c == '\"' || c == ';') {
			return SIZE_MAX;  // illegal characters
		}
		if (++len == MAX_QPATH) {
			return MAX_QPATH;  // oversize value
		}
	}

	return len;
}

/*
==================
Info_SetValueForKey
==================
*/
bool Info_SetValueForKey(char *s, const char *key, const char *value)
{
	char    newi[MAX_INFO_STRING], *v;
	size_t  l, kl, vl;
	int     c;

	// validate key
	kl = Info_SubValidate(key);
	if (kl >= MAX_QPATH) {
		return false;
	}

	// validate value
	vl = Info_SubValidate(value);
	if (vl >= MAX_QPATH) {
		return false;
	}

	Info_RemoveKey(s, key);
	if (!vl) {
		return true;
	}

	l = strlen(s);
	if (l + kl + vl + 2 >= MAX_INFO_STRING) {
		return false;
	}

	newi[0] = '\\';
	memcpy(newi + 1, key, kl);
	newi[kl + 1] = '\\';
	memcpy(newi + kl + 2, value, vl + 1);

	// only copy ascii values
	s += l;
	v = newi;
	while (*v) {
		c = *v++;
		c &= 127;        // strip high bits
		if (isprint(c))
			*s++ = c;
	}
	*s = 0;

	return true;
}