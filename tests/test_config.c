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
 * test_config.c -- ATF unit tests for parse_duration() and
 * parse_skip_transactions().
 *
 * These functions are pure string manipulation with no libbe dependency and
 * no ZFS requirement.  config_register_keys() and config_load() are NOT
 * tested here because they require a live pkg plugin handle.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include <atf-c.h>

/*
 * pkg.h is included for the type definitions (pkg_object_t, pkg_iter, etc.)
 * needed by the stub functions below.  We do NOT link against libpkg; all
 * referenced pkg symbols are stubbed out here.
 */
#include <pkg.h>

#include "pkg-be-plugin.h"
#include "config.h"

/*
 * Stubs for libpkg symbols referenced by config.c.
 *
 * config_register_keys() and config_load() are not called from these unit
 * tests.  The stubs exist solely to satisfy the linker; their bodies are
 * intentionally minimal.
 *
 * pkg_emit_notice() IS called by parse_skip_transactions() when an unknown
 * transaction type token is encountered.  The stub discards the message;
 * tests that care about the warning use ATF_REQUIRE on the resulting flag
 * state, not on the message text.
 */

/* ARGSUSED */
void
pkg_emit_notice(const char *fmt, ...)
{
	(void)fmt;
}

/* ARGSUSED */
void
pkg_plugin_error(struct pkg_plugin *p, const char *fmt, ...)
{
	(void)p;
	(void)fmt;
}

int
pkg_plugin_conf_add(struct pkg_plugin *p, pkg_object_t type,
    const char *key, const char *def)
{
	(void)p;
	(void)type;
	(void)key;
	(void)def;
	return (EPKG_OK);
}

const pkg_object *
pkg_plugin_conf(struct pkg_plugin *p)
{
	(void)p;
	return (NULL);
}

const pkg_object *
pkg_object_iterate(const pkg_object *o, pkg_iter *it)
{
	(void)o;
	(void)it;
	return (NULL);
}

const char *
pkg_object_key(const pkg_object *o)
{
	(void)o;
	return (NULL);
}

bool
pkg_object_bool(const pkg_object *o)
{
	(void)o;
	return (false);
}

int64_t
pkg_object_int(const pkg_object *o)
{
	(void)o;
	return (0);
}

const char *
pkg_object_string(const pkg_object *o)
{
	(void)o;
	return (NULL);
}

/* ------------------------------------------------------------------ */
/* parse_duration tests                                                 */
/* ------------------------------------------------------------------ */

ATF_TC(duration_days);
ATF_TC_HEAD(duration_days, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_duration: days suffix converts correctly");
}
ATF_TC_BODY(duration_days, tc)
{
	time_t	out;

	ATF_REQUIRE_EQ(parse_duration("7d", &out), 0);
	ATF_REQUIRE_EQ(out, 7 * 86400);

	ATF_REQUIRE_EQ(parse_duration("1d", &out), 0);
	ATF_REQUIRE_EQ(out, 86400);

	ATF_REQUIRE_EQ(parse_duration("30d", &out), 0);
	ATF_REQUIRE_EQ(out, 30 * 86400);
}

ATF_TC(duration_hours);
ATF_TC_HEAD(duration_hours, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_duration: hours suffix converts correctly");
}
ATF_TC_BODY(duration_hours, tc)
{
	time_t	out;

	ATF_REQUIRE_EQ(parse_duration("24h", &out), 0);
	ATF_REQUIRE_EQ(out, 86400);

	ATF_REQUIRE_EQ(parse_duration("1h", &out), 0);
	ATF_REQUIRE_EQ(out, 3600);
}

ATF_TC(duration_minutes);
ATF_TC_HEAD(duration_minutes, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_duration: minutes suffix converts correctly");
}
ATF_TC_BODY(duration_minutes, tc)
{
	time_t	out;

	ATF_REQUIRE_EQ(parse_duration("30m", &out), 0);
	ATF_REQUIRE_EQ(out, 1800);

	ATF_REQUIRE_EQ(parse_duration("1m", &out), 0);
	ATF_REQUIRE_EQ(out, 60);
}

ATF_TC(duration_seconds_suffix);
ATF_TC_HEAD(duration_seconds_suffix, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_duration: s suffix converts correctly");
}
ATF_TC_BODY(duration_seconds_suffix, tc)
{
	time_t	out;

	ATF_REQUIRE_EQ(parse_duration("300s", &out), 0);
	ATF_REQUIRE_EQ(out, 300);

	ATF_REQUIRE_EQ(parse_duration("0s", &out), 0);
	ATF_REQUIRE_EQ(out, 0);

	ATF_REQUIRE_EQ(parse_duration("1s", &out), 0);
	ATF_REQUIRE_EQ(out, 1);
}

ATF_TC(duration_bare_seconds);
ATF_TC_HEAD(duration_bare_seconds, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_duration: bare integer treated as seconds");
}
ATF_TC_BODY(duration_bare_seconds, tc)
{
	time_t	out;

	ATF_REQUIRE_EQ(parse_duration("300", &out), 0);
	ATF_REQUIRE_EQ(out, 300);

	ATF_REQUIRE_EQ(parse_duration("0", &out), 0);
	ATF_REQUIRE_EQ(out, 0);

	ATF_REQUIRE_EQ(parse_duration("1", &out), 0);
	ATF_REQUIRE_EQ(out, 1);
}

ATF_TC(duration_errors);
ATF_TC_HEAD(duration_errors, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_duration: invalid inputs return -1");
}
ATF_TC_BODY(duration_errors, tc)
{
	time_t	out;

	/* No digits at all. */
	ATF_REQUIRE_EQ(parse_duration("bad", &out), -1);

	/* Unknown suffix letter. */
	ATF_REQUIRE_EQ(parse_duration("7x", &out), -1);

	/* Suffix with trailing garbage. */
	ATF_REQUIRE_EQ(parse_duration("7d2", &out), -1);
	ATF_REQUIRE_EQ(parse_duration("1hx", &out), -1);

	/* Empty string. */
	ATF_REQUIRE_EQ(parse_duration("", &out), -1);

	/* NULL pointer. */
	ATF_REQUIRE_EQ(parse_duration(NULL, &out), -1);

	/* Negative value. */
	ATF_REQUIRE_EQ(parse_duration("-1", &out), -1);
	ATF_REQUIRE_EQ(parse_duration("-7d", &out), -1);
}

/* ------------------------------------------------------------------ */
/* parse_skip_transactions tests                                        */
/* ------------------------------------------------------------------ */

ATF_TC(skip_empty);
ATF_TC_HEAD(skip_empty, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_skip_transactions: empty string clears all flags");
}
ATF_TC_BODY(skip_empty, tc)
{
	struct be_config	cfg;

	/*
	 * Seed with true to verify the function actually clears the flags
	 * rather than leaving them at whatever they were initialised to.
	 */
	cfg.skip_install = true;
	cfg.skip_upgrade = true;
	cfg.skip_deinstall = true;

	parse_skip_transactions("", &cfg);

	ATF_REQUIRE(!cfg.skip_install);
	ATF_REQUIRE(!cfg.skip_upgrade);
	ATF_REQUIRE(!cfg.skip_deinstall);
}

ATF_TC(skip_null);
ATF_TC_HEAD(skip_null, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_skip_transactions: NULL clears all flags");
}
ATF_TC_BODY(skip_null, tc)
{
	struct be_config	cfg;

	cfg.skip_install = true;
	cfg.skip_upgrade = true;
	cfg.skip_deinstall = true;

	parse_skip_transactions(NULL, &cfg);

	ATF_REQUIRE(!cfg.skip_install);
	ATF_REQUIRE(!cfg.skip_upgrade);
	ATF_REQUIRE(!cfg.skip_deinstall);
}

ATF_TC(skip_deinstall_only);
ATF_TC_HEAD(skip_deinstall_only, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_skip_transactions: single token \"deinstall\"");
}
ATF_TC_BODY(skip_deinstall_only, tc)
{
	struct be_config	cfg;

	parse_skip_transactions("deinstall", &cfg);

	ATF_REQUIRE(!cfg.skip_install);
	ATF_REQUIRE(!cfg.skip_upgrade);
	ATF_REQUIRE(cfg.skip_deinstall);
}

ATF_TC(skip_install_upgrade);
ATF_TC_HEAD(skip_install_upgrade, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_skip_transactions: \"install,upgrade\"");
}
ATF_TC_BODY(skip_install_upgrade, tc)
{
	struct be_config	cfg;

	parse_skip_transactions("install,upgrade", &cfg);

	ATF_REQUIRE(cfg.skip_install);
	ATF_REQUIRE(cfg.skip_upgrade);
	ATF_REQUIRE(!cfg.skip_deinstall);
}

ATF_TC(skip_all);
ATF_TC_HEAD(skip_all, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_skip_transactions: all three types");
}
ATF_TC_BODY(skip_all, tc)
{
	struct be_config	cfg;

	parse_skip_transactions("install,upgrade,deinstall", &cfg);

	ATF_REQUIRE(cfg.skip_install);
	ATF_REQUIRE(cfg.skip_upgrade);
	ATF_REQUIRE(cfg.skip_deinstall);
}

ATF_TC(skip_unknown_token);
ATF_TC_HEAD(skip_unknown_token, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_skip_transactions: unknown token leaves flags false, "
	    "does not abort");
}
ATF_TC_BODY(skip_unknown_token, tc)
{
	struct be_config	cfg;

	/*
	 * "unknown" is not a recognised transaction type.  parse_skip_
	 * transactions() fires pkg_emit_notice() for it and continues.
	 * All flags should remain false.
	 */
	parse_skip_transactions("unknown", &cfg);

	ATF_REQUIRE(!cfg.skip_install);
	ATF_REQUIRE(!cfg.skip_upgrade);
	ATF_REQUIRE(!cfg.skip_deinstall);
}

ATF_TC(skip_mixed_with_unknown);
ATF_TC_HEAD(skip_mixed_with_unknown, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_skip_transactions: valid tokens processed even when "
	    "an unknown token is present");
}
ATF_TC_BODY(skip_mixed_with_unknown, tc)
{
	struct be_config	cfg;

	parse_skip_transactions("install,bogus,deinstall", &cfg);

	ATF_REQUIRE(cfg.skip_install);
	ATF_REQUIRE(!cfg.skip_upgrade);
	ATF_REQUIRE(cfg.skip_deinstall);
}

ATF_TC(skip_whitespace_trimming);
ATF_TC_HEAD(skip_whitespace_trimming, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "parse_skip_transactions: leading whitespace around tokens "
	    "is stripped");
}
ATF_TC_BODY(skip_whitespace_trimming, tc)
{
	struct be_config	cfg;

	/*
	 * Users may write "install, upgrade" with a space after the comma.
	 * strsep() splits on "," so the second token arrives as " upgrade".
	 * The leading-whitespace strip in parse_skip_transactions() handles
	 * this.
	 */
	parse_skip_transactions("install, upgrade", &cfg);

	ATF_REQUIRE(cfg.skip_install);
	ATF_REQUIRE(cfg.skip_upgrade);
	ATF_REQUIRE(!cfg.skip_deinstall);
}

/* ------------------------------------------------------------------ */
/* Test program entry point                                             */
/* ------------------------------------------------------------------ */

ATF_TP_ADD_TCS(tp)
{
	/* parse_duration */
	ATF_TP_ADD_TC(tp, duration_days);
	ATF_TP_ADD_TC(tp, duration_hours);
	ATF_TP_ADD_TC(tp, duration_minutes);
	ATF_TP_ADD_TC(tp, duration_seconds_suffix);
	ATF_TP_ADD_TC(tp, duration_bare_seconds);
	ATF_TP_ADD_TC(tp, duration_errors);

	/* parse_skip_transactions */
	ATF_TP_ADD_TC(tp, skip_empty);
	ATF_TP_ADD_TC(tp, skip_null);
	ATF_TP_ADD_TC(tp, skip_deinstall_only);
	ATF_TP_ADD_TC(tp, skip_install_upgrade);
	ATF_TP_ADD_TC(tp, skip_all);
	ATF_TP_ADD_TC(tp, skip_unknown_token);
	ATF_TP_ADD_TC(tp, skip_mixed_with_unknown);
	ATF_TP_ADD_TC(tp, skip_whitespace_trimming);

	return (atf_no_error());
}
