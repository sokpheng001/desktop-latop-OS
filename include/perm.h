/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * Per-process permissions: what a program may reach outside its own memory. Shared by
 * the kernel and user programs (safe to include from assembly).
 */

#ifndef OS001_PERM_H
#define OS001_PERM_H

/*
 * One bit per resource. A program holds a set of these, fixed by whoever starts it; the
 * kernel checks the set on every system call that touches the resource. A program can
 * give permissions up but never gain one back (like Capsicum's capability mode: Watson
 * et al., "Capsicum: Practical Capabilities for UNIX", USENIX Security 2010). Granting
 * more later is the user's decision, made through the permission manager.
 */
#define PERM_CONSOLE	(1 << 0) /* print on the console */
#define PERM_FILES	(1 << 1) /* open files */
#define PERM_NETWORK	(1 << 2) /* open network connections */
#define PERM_CAMERA	(1 << 3) /* read the camera */
#define PERM_MICROPHONE (1 << 4) /* read the microphone */

#define PERM_COUNT 5
#define PERM_ALL   ((1 << PERM_COUNT) - 1)
#define PERM_NONE  0

#ifndef __ASSEMBLY__

#include "types.h"

/* True if the set held contains every permission in need. */
static inline bool perm_has(u32 held, u32 need)
{
	return (held & need) == need;
}

/* Name of a single permission bit, for reports ("console"); "?" if not one. */
const char *perm_name(u32 perm);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_PERM_H */
