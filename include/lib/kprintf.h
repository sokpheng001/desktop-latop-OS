/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel formatted output. Supports %s %c %d %u %x %p and %%, the l length
 * modifier (long: 64-bit on every OS-001 target), and a minimum field width with optional 0
 * padding.
 */

#ifndef OS001_LIB_KPRINTF_H
#define OS001_LIB_KPRINTF_H

#include "compiler.h"
#include "types.h"

#ifndef __ASSEMBLY__

#include <stdarg.h> /* compiler-provided freestanding header, not libc */

/* Receives every character kprintf produces. */
typedef void (*kprintf_sink_fn)(char c, void *ctx);

/* Where kprintf output goes. Until this is called, kprintf output is dropped. */
void kprintf_set_output(kprintf_sink_fn sink, void *ctx);

/* Format into sink; returns the number of characters produced. */
int kvformat(kprintf_sink_fn sink, void *ctx, const char *fmt, va_list ap);

int kprintf(const char *fmt, ...) ATTR_PRINTF(1, 2);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_LIB_KPRINTF_H */
