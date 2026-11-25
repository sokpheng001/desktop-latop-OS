/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel console: routes kprintf output to the board console through the HAL.
 */

#ifndef OS001_CONSOLE_H
#define OS001_CONSOLE_H

#ifndef __ASSEMBLY__

/* Call after hal_init(). From then on kprintf writes to the board console. */
void console_init(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_CONSOLE_H */
