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
 * prune_testable.c -- pure pruning helpers, free of libbe, libpkg, and ZFS.
 *
 * Single source of truth for the pruning sort, prefix match, and selection
 * logic.  Compiled into both be.so (prune.c calls cand_sort() and
 * be_prune_select() directly) and the test_prune binary (which exercises the
 * same functions without a live ZFS pool).  All libbe side effects -- handle
 * management, enumeration, be_destroy() -- stay in prune.c; everything here is
 * referentially transparent and suitable for WARNS=6.
 */

#include <sys/cdefs.h>

#include <stdbool.h>
#include <stdint.h>
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
	const struct be_candidate *ca = a;
	const struct be_candidate *cb = b;

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
	size_t		prefix_len;

	if (name == NULL || *name == '\0' || prefix == NULL)
		return (false);

	prefix_len = strlen(prefix);
	return (strncmp(name, prefix, prefix_len) == 0 &&
	    name[prefix_len] == '-');
}

size_t
be_prune_select(const struct be_candidate *cands, size_t n_cands,
    int64_t keep, time_t min_age, time_t now,
    be_destroy_fn destroy, be_defer_fn defer, void *ctx)
{
	size_t		i, n_delete, destroyed;

	if (cands == NULL || keep < 0 || (int64_t)n_cands <= keep)
		return (0);

	n_delete = (size_t)((int64_t)n_cands - keep);
	destroyed = 0;

	for (i = 0; i < n_delete; i++) {
		if (min_age > 0 && now - cands[i].creation < min_age) {
			/*
			 * This candidate is younger than min_age.  Because the
			 * array is sorted oldest-first, every later candidate is
			 * younger still -- defer the whole remainder and stop.
			 */
			if (defer != NULL)
				defer(ctx, n_delete - i);
			break;
		}

		if (destroy != NULL)
			destroy(ctx, cands[i].name);
		destroyed++;
	}

	return (destroyed);
}
