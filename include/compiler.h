/*
 * Project: OS-001
 * Authors: kwan, oda, sokpheng001
 *
 * Compiler attributes and helper macros. This is the only place GCC extensions
 * (__attribute__, __builtin_*) are allowed to appear directly (CONTRIBUTING.md §3).
 */

#ifndef OS001_COMPILER_H
#define OS001_COMPILER_H

#ifndef __ASSEMBLY__

/* Attributes */
#define ATTR_PACKED	   __attribute__((packed))
#define ATTR_ALIGNED(n)	   __attribute__((aligned(n)))
#define ATTR_NORETURN	   __attribute__((noreturn))
#define ATTR_UNUSED	   __attribute__((unused))
#define ATTR_USED	   __attribute__((used))
#define ATTR_WEAK	   __attribute__((weak))
#define ATTR_SECTION(s)	   __attribute__((section(s)))
#define ATTR_NOINLINE	   __attribute__((noinline))
#define ATTR_ALWAYS_INLINE inline __attribute__((always_inline))
#define ATTR_PRINTF(f, a)  __attribute__((format(printf, f, a)))

/* Branch prediction hints for hot paths. */
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

/* Stops the compiler from moving memory accesses across this point (no CPU barrier). */
#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")

#define UNREACHABLE() __builtin_unreachable()

#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

/* Number of elements in a true array (not a pointer). */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define OFFSET_OF(type, member) __builtin_offsetof(type, member)

/* Pointer to the structure that contains the given member. */
#define CONTAINER_OF(ptr, type, member) ((type *)((char *)(ptr) - OFFSET_OF(type, member)))

#endif /* !__ASSEMBLY__ */

#endif /* OS001_COMPILER_H */
