/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Byte-wise memset, memcpy and strlen. Simple and correct first; word-sized
 * fast paths can come later when profiling shows they matter.
 */

#include "lib/string.h"

void *memset(void *dst, int value, usize len)
{
	u8 *d = dst;

	while (len--)
		*d++ = (u8)value;
	return dst;
}

void *memcpy(void *restrict dst, const void *restrict src, usize len)
{
	u8 *d = dst;
	const u8 *s = src;

	while (len--)
		*d++ = *s++;
	return dst;
}

usize strlen(const char *s)
{
	const char *p = s;

	while (*p)
		p++;
	return (usize)(p - s);
}
