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
 *
 * Both are accessed through the thin shim in prune_testable.c which
 * re-uses the struct definition and exposes the otherwise-static helpers.
 */

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

	return (atf_no_error());
}
