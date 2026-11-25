/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Hardware abstraction layer. kernel/ talks to hardware only through these
 * functions; each board implements them in board/<board>/.
 */

#ifndef OS001_HAL_H
#define OS001_HAL_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Bring up the board's early devices (console first). Called once, from kernel_main. */
void hal_init(void);

/* Write one raw byte to the console, waiting while the transmitter is busy. */
void hal_console_putc(char c);

/* Read one byte from the console without waiting. Returns -1 if none is available. */
int hal_console_getc(void);

/* Short, stable board identifier, e.g. "qemu-virt" (shown at boot and in logs). */
const char *hal_board_name(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_HAL_H */
