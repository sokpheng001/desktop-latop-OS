/*
 * Project: OS-001
 * Date: 2026-11-27
 * Authors: kwan, oda, sokpheng001
 *
 * Names of the per-process permissions, for kernel reports.
 */

#include "perm.h"

static const char *const perm_names[PERM_COUNT] = {
    "console", "files", "network", "camera", "microphone",
};

const char *perm_name(u32 perm)
{
	for (int i = 0; i < PERM_COUNT; i++)
		if (perm == (1u << i))
			return perm_names[i];
	return "?";
}
