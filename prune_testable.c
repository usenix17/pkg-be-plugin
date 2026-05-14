/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Sasha Karcz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * prune_testable.c -- thin shim exposing pure pruning helpers for ATF tests.
 *
 * This file provides cand_sort() and be_name_matches_prefix() without any
 * dependency on libbe, libpkg, or ZFS.  It is compiled into the test binary
 * (test_prune) only; it is NOT linked into be.so.
 *
 * The implementations here mirror the logic in prune.c exactly.  If the
 * algorithm changes in prune.c, update both files.  The duplication is
 * intentional: prune.c must remain self-contained (no extra headers) and
 * suitable for WARNS=6 in the shared library build.
 */

#include <sys/cdefs.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "prune_testable.h"

/*
 * cand_cmp -- oldest creation timestamp first.
 */
static int
cand_cmp(const void *a, const void *b)
{
	const struct be_candidate	*ca = a;
	const struct be_candidate	*cb = b;

	if (ca->creation < cb->creation)
		return (-1);
	if (ca->creation > cb->creation)
		return (1);
	return (0);
}

void
cand_sort(struct be_candidate *arr, size_t n)
{
	qsort(arr, n, sizeof(*arr), cand_cmp);
}

bool
be_name_matches_prefix(const char *name, const char *prefix)
{
	size_t	prefix_len;

	if (name == NULL || *name == '\0' || prefix == NULL)
		return (false);

	prefix_len = strlen(prefix);
	return (strncmp(name, prefix, prefix_len) == 0 &&
	    name[prefix_len] == '-');
}
