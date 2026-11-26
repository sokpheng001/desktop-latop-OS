/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel console: routes kprintf output to the board console through the HAL.
 */

#ifndef OS001_CONSOLE_H
#define OS001_CONSOLE_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Call after hal_init(). From then on kprintf writes to the board console. */
void console_init(void);

/* Write len bytes to the console exactly as kprintf would (newline becomes CR+LF). */
void console_write(const char *buf, usize len);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_CONSOLE_H */
