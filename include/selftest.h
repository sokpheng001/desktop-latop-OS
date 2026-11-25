/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Built-in kernel self-tests, selected at build time with make KERNEL_TEST=<name>.
 */

#ifndef OS001_SELFTEST_H
#define OS001_SELFTEST_H

#ifndef __ASSEMBLY__

/* Runs the self-test this kernel was built with, if any. Normal builds do nothing. */
void selftest_run(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_SELFTEST_H */
