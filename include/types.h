/*
 * Project: OS-001
 * Authors: kwan, oda, sokpheng001
 *
 * Fixed-width integer types and basic definitions for freestanding kernel code.
 * Built on compiler-provided type macros so no libc headers are needed.
 */

#ifndef OS001_TYPES_H
#define OS001_TYPES_H

#ifndef __ASSEMBLY__

typedef __UINT8_TYPE__ u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;

typedef __INT8_TYPE__ s8;
typedef __INT16_TYPE__ s16;
typedef __INT32_TYPE__ s32;
typedef __INT64_TYPE__ s64;

typedef __SIZE_TYPE__ usize;
typedef __PTRDIFF_TYPE__ ssize;
typedef __UINTPTR_TYPE__ uptr;

/* bool, true and false are keywords from C23 on; define them for C17. */
#if __STDC_VERSION__ < 202311L
typedef _Bool bool;
#define true  1
#define false 0
#endif

#define NULL ((void *)0)

/* Every file that uses these types also checks the sizes the kernel relies on. */
_Static_assert(sizeof(u8) == 1, "u8 must be 1 byte");
_Static_assert(sizeof(u16) == 2, "u16 must be 2 bytes");
_Static_assert(sizeof(u32) == 4, "u32 must be 4 bytes");
_Static_assert(sizeof(u64) == 8, "u64 must be 8 bytes");
_Static_assert(sizeof(usize) == sizeof(void *), "usize must hold a pointer-sized value");
_Static_assert(sizeof(uptr) == sizeof(void *), "uptr must hold a pointer");

#endif /* !__ASSEMBLY__ */

#endif /* OS001_TYPES_H */
