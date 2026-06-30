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
 * test_naming.c -- ATF unit tests for be_disambiguate_name() (be_naming.c).
 *
 * The function's only libbe dependency -- "does this BE name already exist?"
 * -- is supplied here through a be_name_taken_fn callback backed by an
 * in-memory fixture, so the same-second collision logic is exercised with no
 * libbe or ZFS present.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <atf-c.h>

#include "be_naming.h"

#define	BASE	"pre-pkg-20260513-142301"

/*
 * taken_fixture -- a NULL-terminated list of names considered "taken",
 * plus a call counter so tests can assert how often the predicate ran.
 */
struct taken_fixture {
	const char *const	*names;
	size_t			 calls;
};

/* be_name_taken_fn: true when name appears in the fixture's list. */
static bool
taken_in_list(void *ctx, const char *name)
{
	struct taken_fixture	*f = ctx;
	size_t			 i;

	f->calls++;
	for (i = 0; f->names[i] != NULL; i++) {
		if (strcmp(f->names[i], name) == 0)
			return (true);
	}
	return (false);
}

/* be_name_taken_fn: every name is taken (drives the exhaustion path). */
static bool
taken_always(void *ctx, const char *name)
{
	size_t	*calls = ctx;

	(void)name;
	(*calls)++;
	return (true);
}

/* ------------------------------------------------------------------ */

ATF_TC(free_name_unchanged);
ATF_TC_HEAD(free_name_unchanged, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_disambiguate_name: a free name is returned unchanged after a "
	    "single existence check");
}
ATF_TC_BODY(free_name_unchanged, tc)
{
	static const char *const	none[] = { NULL };
	struct taken_fixture		f = { none, 0 };
	char				buf[128];

	(void)strlcpy(buf, BASE, sizeof(buf));

	ATF_REQUIRE(be_disambiguate_name(taken_in_list, &f, buf, sizeof(buf)));
	ATF_REQUIRE_STREQ(buf, BASE);
	ATF_REQUIRE_EQ(f.calls, 1);
}

ATF_TC(single_collision_gets_2);
ATF_TC_HEAD(single_collision_gets_2, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_disambiguate_name: a taken base name yields \"<base>-2\"");
}
ATF_TC_BODY(single_collision_gets_2, tc)
{
	static const char *const	taken[] = { BASE, NULL };
	struct taken_fixture		f = { taken, 0 };
	char				buf[128];

	(void)strlcpy(buf, BASE, sizeof(buf));

	ATF_REQUIRE(be_disambiguate_name(taken_in_list, &f, buf, sizeof(buf)));
	ATF_REQUIRE_STREQ(buf, BASE "-2");
}

ATF_TC(cascade_skips_to_first_free);
ATF_TC_HEAD(cascade_skips_to_first_free, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_disambiguate_name: base, -2 and -3 taken yields -4, and the "
	    "suffix is appended to the original base (never -2-3)");
}
ATF_TC_BODY(cascade_skips_to_first_free, tc)
{
	static const char *const	taken[] = {
		BASE, BASE "-2", BASE "-3", NULL
	};
	struct taken_fixture		f = { taken, 0 };
	char				buf[128];

	(void)strlcpy(buf, BASE, sizeof(buf));

	ATF_REQUIRE(be_disambiguate_name(taken_in_list, &f, buf, sizeof(buf)));
	ATF_REQUIRE_STREQ(buf, BASE "-4");
}

ATF_TC(exhausted_returns_false);
ATF_TC_HEAD(exhausted_returns_false, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_disambiguate_name: when every candidate through 99 is taken it "
	    "returns false and leaves the -99 candidate in buf");
}
ATF_TC_BODY(exhausted_returns_false, tc)
{
	size_t	calls = 0;
	char	buf[128];

	(void)strlcpy(buf, BASE, sizeof(buf));

	ATF_REQUIRE(!be_disambiguate_name(taken_always, &calls, buf,
	    sizeof(buf)));
	ATF_REQUIRE_STREQ(buf, BASE "-99");
	/* One probe for the base plus N=2..99 inclusive = 99 checks. */
	ATF_REQUIRE_EQ(calls, 99);
}

ATF_TC(null_predicate_is_noop);
ATF_TC_HEAD(null_predicate_is_noop, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "be_disambiguate_name: a NULL predicate leaves buf unchanged and "
	    "reports the name as free");
}
ATF_TC_BODY(null_predicate_is_noop, tc)
{
	char	buf[128];

	(void)strlcpy(buf, BASE, sizeof(buf));

	ATF_REQUIRE(be_disambiguate_name(NULL, NULL, buf, sizeof(buf)));
	ATF_REQUIRE_STREQ(buf, BASE);
}

/* ------------------------------------------------------------------ */

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, free_name_unchanged);
	ATF_TP_ADD_TC(tp, single_collision_gets_2);
	ATF_TP_ADD_TC(tp, cascade_skips_to_first_free);
	ATF_TP_ADD_TC(tp, exhausted_returns_false);
	ATF_TP_ADD_TC(tp, null_predicate_is_noop);

	return (atf_no_error());
}
