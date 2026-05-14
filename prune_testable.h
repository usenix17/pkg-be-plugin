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

#ifndef PRUNE_TESTABLE_H
#define	PRUNE_TESTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/*
 * be_candidate: the per-BE record used by the pruning sort.
 *
 * Duplicated here (rather than in prune.h) so that test code can
 * allocate and inspect arrays without dragging in libbe headers.
 * The size constants must match those in prune.c.
 */
#define	BE_CAND_NAME_MAX	256

struct be_candidate {
	char	name[BE_CAND_NAME_MAX];
	time_t	creation;
};

/*
 * cand_sort -- sort an array of be_candidate oldest-first.
 * Wraps qsort with the internal cand_cmp comparator.
 */
void	cand_sort(struct be_candidate *arr, size_t n);

/*
 * be_name_matches_prefix -- return true if name starts with "<prefix>-".
 * Encapsulates the prefix-matching logic used when collecting candidates.
 */
bool	be_name_matches_prefix(const char *name, const char *prefix);

#endif	/* PRUNE_TESTABLE_H */
