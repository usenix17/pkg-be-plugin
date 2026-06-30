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
#include <stdint.h>
#include <time.h>

/*
 * be_candidate: the per-BE record used by the pruning sort.
 *
 * This header is the single definition of the candidate record and the pure
 * pruning helpers below.  prune_testable.c is compiled into both be.so (so
 * prune.c shares this code rather than duplicating it) and the test_prune
 * binary (so the helpers can be exercised without libbe or ZFS).
 */
#define	BE_CAND_NAME_MAX	256

struct be_candidate {
	char		name[BE_CAND_NAME_MAX];
	time_t		creation;
};

/*
 * cand_sort -- sort an array of be_candidate oldest-first.
 * Wraps qsort with the internal cand_cmp comparator.
 */
void		cand_sort(struct be_candidate *arr, size_t n);

/*
 * be_name_matches_prefix -- return true if name starts with "<prefix>-".
 * Encapsulates the prefix-matching logic used when collecting candidates.
 */
bool		be_name_matches_prefix(const char *name, const char *prefix);

/*
 * be_destroy_fn / be_defer_fn -- callbacks forming the be_prune_select() seam.
 *
 * be_destroy_fn is invoked once per BE selected for destruction (production
 * wraps be_destroy(); tests record the call).  be_defer_fn is invoked at most
 * once when one or more over-limit BEs are held back for being younger than
 * min_age, with the count still pending (production emits a deferred notice;
 * tests record it).  ctx is an opaque pass-through for both.
 */
typedef void (*be_destroy_fn)(void *ctx, const char *name);
typedef void (*be_defer_fn)(void *ctx, size_t remaining);

/*
 * be_prune_select -- decide and drive destruction over sorted candidates.
 *
 * cands must already be sorted oldest-first.  The oldest (n_cands - keep)
 * BEs exceed the retention target; this walks them oldest-first and invokes
 * destroy() for each whose age (now - creation) is at least min_age.  At the
 * first candidate younger than min_age it invokes defer() once with the number
 * of BEs left un-pruned and stops (all later candidates are newer still).  A
 * min_age <= 0 disables the age check.  Returns the number of destroy() calls
 * made.
 *
 * Pure: no libbe, no syslog, no globals -- the libbe and pkg side effects live
 * entirely in the caller-supplied callbacks.
 */
size_t
be_prune_select(const struct be_candidate *cands, size_t n_cands,
    int64_t keep, time_t min_age, time_t now,
    be_destroy_fn destroy, be_defer_fn defer, void *ctx);

#endif				/* PRUNE_TESTABLE_H */
