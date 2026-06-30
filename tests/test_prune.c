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
 * test_prune.c -- ATF unit tests for pruning sort order and
 * candidate-selection helpers exposed via prune_testable.c.
 *
 * prune_old_bes() itself requires a live ZFS boot environment and is not
 * tested here.  We test the two pieces of pure logic that can run without
 * ZFS:
 *
 *   1. cand_sort: qsort of a be_candidate array sorts oldest-first.
 *   2. be_name_matches_prefix: the "<prefix>-..." name check.
 *   3. be_prune_select: which sorted candidates get destroyed, and when the
 *      min_age cutoff defers the remainder, driven through recording mocks
 *      that stand in for be_destroy() and the deferred notice.
 *
 * All are accessed through prune_testable.c, the same source that prune.c
 * itself links, so these tests exercise the production logic directly.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <atf-c.h>

#include "prune_testable.h"

/* ------------------------------------------------------------------ */
/* cand_sort tests                                                       */
/* ------------------------------------------------------------------ */

ATF_TC(sort_already_sorted);
ATF_TC_HEAD(sort_already_sorted, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "cand_sort: already-sorted array stays sorted");
}
ATF_TC_BODY(sort_already_sorted, tc)
{
	struct be_candidate	arr[3];

	arr[0].creation = 1000;
	arr[1].creation = 2000;
	arr[2].creation = 3000;
	(void)strlcpy(arr[0].name, "pre-pkg-19700101-000000",
	    sizeof(arr[0].name));
	(void)strlcpy(arr[1].name, "pre-pkg-19700101-000100",
	    sizeof(arr[1].name));
	(void)strlcpy(arr[2].name, "pre-pkg-19700101-000200",
	    sizeof(arr[2].name));

	cand_sort(arr, 3);

	ATF_REQUIRE_EQ(arr[0].creation, 1000);
	ATF_REQUIRE_EQ(arr[1].creation, 2000);
	ATF_REQUIRE_EQ(arr[2].creation, 3000);
}

ATF_TC(sort_reverse_order);
ATF_TC_HEAD(sort_reverse_order, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "cand_sort: newest-first input is reversed to oldest-first");
}
ATF_TC_BODY(sort_reverse_order, tc)
{
	struct be_candidate	arr[3];

	arr[0].creation = 3000;
	arr[1].creation = 2000;
	arr[2].creation = 1000;

	cand_sort(arr, 3);

	ATF_REQUIRE_EQ(arr[0].creation, 1000);
	ATF_REQUIRE_EQ(arr[1].creation, 2000);
	ATF_REQUIRE_EQ(arr[2].creation, 3000);
}

ATF_TC(sort_ties_stable_order);
ATF_TC_HEAD(sort_ties_stable_order, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "cand_sort: equal timestamps produce a defined ordering");
}
ATF_TC_BODY(sort_ties_stable_order, tc)
{
	struct be_candidate	arr[3];

	arr[0].creation = 5000;
	arr[1].creation = 5000;
	arr[2].creation = 5000;

	/* No assertion on relative order -- just must not crash. */
	cand_sort(arr, 3);

	ATF_REQUIRE_EQ(arr[0].creation, 5000);
	ATF_REQUIRE_EQ(arr[1].creation, 5000);
	ATF_REQUIRE_EQ(arr[2].creation, 5000);
}

ATF_TC(sort_single_element);
ATF_TC_HEAD(sort_single_element, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "cand_sort: single-element array is a no-op");
}
ATF_TC_BODY(sort_single_element, tc)
{
	struct be_candidate	arr[1];

	arr[0].creation = 9999;
	cand_sort(arr, 1);
	ATF_REQUIRE_EQ(arr[0].creation, 9999);
}

ATF_TC(sort_random_order);
ATF_TC_HEAD(sort_random_order, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "cand_sort: unsorted five-element array comes out oldest-first");
}
ATF_TC_BODY(sort_random_order, tc)
{
	struct be_candidate	arr[5];
	size_t			i;

	arr[0].creation = 500;
	arr[1].creation = 100;
	arr[2].creation = 900;
	arr[3].creation = 300;
	arr[4].creation = 700;

	cand_sort(arr, 5);

	/* Verify strictly ascending. */
	for (i = 0; i + 1 < 5; i++)
		ATF_REQUIRE(arr[i].creation <= arr[i + 1].creation);
	ATF_REQUIRE_EQ(arr[0].creation, 100);
	ATF_REQUIRE_EQ(arr[4].creation, 900);
}

/* ------------------------------------------------------------------ */
/* be_name_matches_prefix tests                                          */
/* ------------------------------------------------------------------ */

ATF_TC(prefix_match_basic);
ATF_TC_HEAD(prefix_match_basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_name_matches_prefix: standard generated name matches");
}
ATF_TC_BODY(prefix_match_basic, tc)
{
	ATF_REQUIRE(be_name_matches_prefix("pre-pkg-20260513-142301",
	    "pre-pkg"));
}

ATF_TC(prefix_match_no_dash);
ATF_TC_HEAD(prefix_match_no_dash, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_name_matches_prefix: name equal to prefix (no trailing dash) "
	    "does not match");
}
ATF_TC_BODY(prefix_match_no_dash, tc)
{
	/* "pre-pkg" by itself has no '-' after the prefix. */
	ATF_REQUIRE(!be_name_matches_prefix("pre-pkg", "pre-pkg"));
}

ATF_TC(prefix_match_wrong_prefix);
ATF_TC_HEAD(prefix_match_wrong_prefix, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_name_matches_prefix: different prefix does not match");
}
ATF_TC_BODY(prefix_match_wrong_prefix, tc)
{
	ATF_REQUIRE(!be_name_matches_prefix("pre-pkg-20260513-142301",
	    "other"));
}

ATF_TC(prefix_match_superstring);
ATF_TC_HEAD(prefix_match_superstring, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_name_matches_prefix: prefix that is a superstring of the "
	    "name does not match");
}
ATF_TC_BODY(prefix_match_superstring, tc)
{
	/*
	 * "pre-pkg-extra" is a longer prefix than the name "pre-pkg-2026".
	 * strncmp would compare more bytes than the name has, so this must
	 * not match.
	 */
	ATF_REQUIRE(!be_name_matches_prefix("pre-pkg-2026",
	    "pre-pkg-extra"));
}

ATF_TC(prefix_match_partial_word);
ATF_TC_HEAD(prefix_match_partial_word, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_name_matches_prefix: prefix \"pre\" does not match "
	    "\"pre-pkg-...\" because the char after prefix is '-', "
	    "but prefix itself ends at 'e' so position prefix_len IS '-': "
	    "must match");
}
ATF_TC_BODY(prefix_match_partial_word, tc)
{
	/*
	 * prefix="pre", name="pre-20260513-142301".
	 * name[3] == '-', so this should match.
	 */
	ATF_REQUIRE(be_name_matches_prefix("pre-20260513-142301", "pre"));

	/*
	 * prefix="pre-pkg", name="pre-pkg-extra-20260513-142301".
	 * A user BE named "pre-pkg-extra" happens to start with our prefix.
	 * name[7] == '-', so this matches too -- intentional: we manage any
	 * BE whose name starts with "<prefix>-".
	 */
	ATF_REQUIRE(be_name_matches_prefix(
	    "pre-pkg-extra-20260513-142301", "pre-pkg"));
}

ATF_TC(prefix_match_empty_name);
ATF_TC_HEAD(prefix_match_empty_name, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_name_matches_prefix: empty BE name does not match");
}
ATF_TC_BODY(prefix_match_empty_name, tc)
{
	ATF_REQUIRE(!be_name_matches_prefix("", "pre-pkg"));
}

/* ------------------------------------------------------------------ */
/* be_prune_select tests                                                 */
/* ------------------------------------------------------------------ */

/*
 * prune_record -- captures the destroy()/defer() callbacks so a test can
 * assert exactly which BEs were selected, in order, and whether the min_age
 * cutoff deferred any remainder.
 */
#define	REC_MAX	16

struct prune_record {
	char	destroyed[REC_MAX][BE_CAND_NAME_MAX];
	size_t	n_destroyed;
	size_t	defer_calls;
	size_t	defer_remaining;
};

static void
rec_destroy(void *ctx, const char *name)
{
	struct prune_record	*r = ctx;

	if (r->n_destroyed < REC_MAX)
		(void)strlcpy(r->destroyed[r->n_destroyed], name,
		    BE_CAND_NAME_MAX);
	r->n_destroyed++;
}

static void
rec_defer(void *ctx, size_t remaining)
{
	struct prune_record	*r = ctx;

	r->defer_calls++;
	r->defer_remaining = remaining;
}

static void
set_cand(struct be_candidate *c, const char *name, time_t creation)
{
	(void)strlcpy(c->name, name, sizeof(c->name));
	c->creation = creation;
}

ATF_TC(select_within_keep_noop);
ATF_TC_HEAD(select_within_keep_noop, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_prune_select: count <= keep destroys nothing and never defers");
}
ATF_TC_BODY(select_within_keep_noop, tc)
{
	struct be_candidate	cands[3];
	struct prune_record	r = { { { 0 } }, 0, 0, 0 };
	size_t			destroyed;

	set_cand(&cands[0], "pre-pkg-a", 1000);
	set_cand(&cands[1], "pre-pkg-b", 2000);
	set_cand(&cands[2], "pre-pkg-c", 3000);

	/* keep == count is still within the limit. */
	destroyed = be_prune_select(cands, 3, 3, 0, 100000,
	    rec_destroy, rec_defer, &r);

	ATF_REQUIRE_EQ(destroyed, 0);
	ATF_REQUIRE_EQ(r.n_destroyed, 0);
	ATF_REQUIRE_EQ(r.defer_calls, 0);
}

ATF_TC(select_oldest_first);
ATF_TC_HEAD(select_oldest_first, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_prune_select: with min_age disabled the oldest (count-keep) "
	    "BEs are destroyed in oldest-first order");
}
ATF_TC_BODY(select_oldest_first, tc)
{
	struct be_candidate	cands[5];
	struct prune_record	r = { { { 0 } }, 0, 0, 0 };
	size_t			destroyed;

	set_cand(&cands[0], "pre-pkg-1000", 1000);
	set_cand(&cands[1], "pre-pkg-2000", 2000);
	set_cand(&cands[2], "pre-pkg-3000", 3000);
	set_cand(&cands[3], "pre-pkg-4000", 4000);
	set_cand(&cands[4], "pre-pkg-5000", 5000);

	destroyed = be_prune_select(cands, 5, 2, 0, 100000,
	    rec_destroy, rec_defer, &r);

	ATF_REQUIRE_EQ(destroyed, 3);
	ATF_REQUIRE_EQ(r.n_destroyed, 3);
	ATF_REQUIRE_EQ(r.defer_calls, 0);
	ATF_REQUIRE_STREQ(r.destroyed[0], "pre-pkg-1000");
	ATF_REQUIRE_STREQ(r.destroyed[1], "pre-pkg-2000");
	ATF_REQUIRE_STREQ(r.destroyed[2], "pre-pkg-3000");
}

ATF_TC(select_min_age_defers_all);
ATF_TC_HEAD(select_min_age_defers_all, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_prune_select: when every over-limit BE is younger than min_age, "
	    "nothing is destroyed and defer() fires once for the whole set");
}
ATF_TC_BODY(select_min_age_defers_all, tc)
{
	struct be_candidate	cands[5];
	struct prune_record	r = { { { 0 } }, 0, 0, 0 };
	size_t			destroyed;

	/* now=10000, min_age=1000; all creations are within 1000s of now. */
	set_cand(&cands[0], "pre-pkg-a", 9500);
	set_cand(&cands[1], "pre-pkg-b", 9600);
	set_cand(&cands[2], "pre-pkg-c", 9700);
	set_cand(&cands[3], "pre-pkg-d", 9800);
	set_cand(&cands[4], "pre-pkg-e", 9900);

	destroyed = be_prune_select(cands, 5, 2, 1000, 10000,
	    rec_destroy, rec_defer, &r);

	ATF_REQUIRE_EQ(destroyed, 0);
	ATF_REQUIRE_EQ(r.n_destroyed, 0);
	ATF_REQUIRE_EQ(r.defer_calls, 1);
	ATF_REQUIRE_EQ(r.defer_remaining, 3);	/* count - keep = 5 - 2 */
}

ATF_TC(select_min_age_partial);
ATF_TC_HEAD(select_min_age_partial, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_prune_select: old-enough BEs are destroyed and the first "
	    "too-young BE defers exactly the remaining count");
}
ATF_TC_BODY(select_min_age_partial, tc)
{
	struct be_candidate	cands[5];
	struct prune_record	r = { { { 0 } }, 0, 0, 0 };
	size_t			destroyed;

	/*
	 * now=10000, min_age=1000, keep=1 -> 4 candidates are over the limit.
	 * Ages: 9000, 8000, 7000 (>= min_age, destroyed); then 500 (< min_age,
	 * defers the remainder).  The fifth is retained by keep.
	 */
	set_cand(&cands[0], "pre-pkg-1000", 1000);
	set_cand(&cands[1], "pre-pkg-2000", 2000);
	set_cand(&cands[2], "pre-pkg-3000", 3000);
	set_cand(&cands[3], "pre-pkg-9500", 9500);
	set_cand(&cands[4], "pre-pkg-9600", 9600);

	destroyed = be_prune_select(cands, 5, 1, 1000, 10000,
	    rec_destroy, rec_defer, &r);

	ATF_REQUIRE_EQ(destroyed, 3);
	ATF_REQUIRE_EQ(r.n_destroyed, 3);
	ATF_REQUIRE_STREQ(r.destroyed[2], "pre-pkg-3000");
	ATF_REQUIRE_EQ(r.defer_calls, 1);
	ATF_REQUIRE_EQ(r.defer_remaining, 1);	/* n_delete(4) - i(3) */
}

ATF_TC(select_negative_keep_noop);
ATF_TC_HEAD(select_negative_keep_noop, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_prune_select: a negative keep is rejected, destroying nothing");
}
ATF_TC_BODY(select_negative_keep_noop, tc)
{
	struct be_candidate	cands[2];
	struct prune_record	r = { { { 0 } }, 0, 0, 0 };

	set_cand(&cands[0], "pre-pkg-a", 1000);
	set_cand(&cands[1], "pre-pkg-b", 2000);

	ATF_REQUIRE_EQ(be_prune_select(cands, 2, -1, 0, 100000,
	    rec_destroy, rec_defer, &r), 0);
	ATF_REQUIRE_EQ(r.n_destroyed, 0);
	ATF_REQUIRE_EQ(r.defer_calls, 0);
}

/* ------------------------------------------------------------------ */
/* Test program entry point                                              */
/* ------------------------------------------------------------------ */

ATF_TP_ADD_TCS(tp)
{
	/* cand_sort */
	ATF_TP_ADD_TC(tp, sort_already_sorted);
	ATF_TP_ADD_TC(tp, sort_reverse_order);
	ATF_TP_ADD_TC(tp, sort_ties_stable_order);
	ATF_TP_ADD_TC(tp, sort_single_element);
	ATF_TP_ADD_TC(tp, sort_random_order);

	/* be_name_matches_prefix */
	ATF_TP_ADD_TC(tp, prefix_match_basic);
	ATF_TP_ADD_TC(tp, prefix_match_no_dash);
	ATF_TP_ADD_TC(tp, prefix_match_wrong_prefix);
	ATF_TP_ADD_TC(tp, prefix_match_superstring);
	ATF_TP_ADD_TC(tp, prefix_match_partial_word);
	ATF_TP_ADD_TC(tp, prefix_match_empty_name);

	/* be_prune_select */
	ATF_TP_ADD_TC(tp, select_within_keep_noop);
	ATF_TP_ADD_TC(tp, select_oldest_first);
	ATF_TP_ADD_TC(tp, select_min_age_defers_all);
	ATF_TP_ADD_TC(tp, select_min_age_partial);
	ATF_TP_ADD_TC(tp, select_negative_keep_noop);

	return (atf_no_error());
}
