/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Freestanding memory and string helpers (no libc).
 */

#ifndef OS001_LIB_STRING_H
#define OS001_LIB_STRING_H

#include "types.h"

#ifndef __ASSEMBLY__

/*
 * memset and memcpy keep their standard names and signatures: the compiler may
 * emit calls to them on its own (e.g. for struct copies), even in freestanding code.
 */
void *memset(void *dst, int value, usize len);
void *memcpy(void *restrict dst, const void *restrict src, usize len);
usize strlen(const char *s);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_LIB_STRING_H */
