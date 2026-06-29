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
 * config.c -- plugin configuration helpers.
 *
 * config_register_keys() declares all config keys and their compiled-in
 * defaults to libpkg via pkg_plugin_conf_add().  pkg_plugin_init() then
 * calls pkg_plugin_parse() to overlay those defaults with values from the
 * UCL config file (typically /usr/local/etc/pkg/be.conf), and finally
 * calls config_load() to read the resulting merged object into g_config.
 *
 * parse_duration() and parse_skip_transactions() are exposed in config.h
 * because they are tested independently via ATF without needing a live pkg
 * context.
 */

#include <sys/cdefs.h>

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <pkg.h>

#include "pkg-be-plugin.h"
#include "config.h"

/*
 * Compiled-in defaults.  These are also passed as the def argument to
 * pkg_plugin_conf_add(), so they appear in the config object even when the
 * user has no config file.
 */
#define	DEFAULT_ENABLED		"true"
#define	DEFAULT_KEEP		"5"
#define	DEFAULT_PREFIX		"pre-pkg"
#define	DEFAULT_MIN_AGE		"7d"
#define	DEFAULT_STRICT		"false"
#define	DEFAULT_SKIP		""
#define	DEFAULT_REPOSITORIES	""

/*
 * parse_duration -- convert a duration string to seconds.
 *
 * Accepted forms:
 *   "7d"   -- days    (n * 86400)
 *   "24h"  -- hours   (n * 3600)
 *   "30m"  -- minutes (n * 60)
 *   "300s" -- seconds (n * 1)
 *   "300"  -- bare integer, interpreted as seconds
 *   "0"    -- zero (disables minimum age)
 *
 * Any trailing characters after the optional suffix letter are rejected.
 * Negative values are rejected.
 *
 * Returns 0 on success with *out set, -1 on parse error.
 */
int
parse_duration(const char *s, time_t *out)
{
	char	*end;
	long	 n;

	if (s == NULL || *s == '\0')
		return (-1);

	errno = 0;
	n = strtol(s, &end, 10);
	if (end == s || errno != 0 || n < 0)
		return (-1);

	switch (*end) {
	case 'd':
		if (*(end + 1) != '\0')
			return (-1);
		*out = (time_t)n * 86400;
		break;
	case 'h':
		if (*(end + 1) != '\0')
			return (-1);
		*out = (time_t)n * 3600;
		break;
	case 'm':
		if (*(end + 1) != '\0')
			return (-1);
		*out = (time_t)n * 60;
		break;
	case 's':
		if (*(end + 1) != '\0')
			return (-1);
		*out = (time_t)n;
		break;
	case '\0':
		*out = (time_t)n;
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * parse_skip_transactions -- parse a comma-separated list of transaction type
 * names and set the corresponding skip_* flags in *cfg.
 *
 * Recognised tokens (case-sensitive): "install", "upgrade", "deinstall".
 * Unknown tokens produce a pkg_emit_notice() warning but are not fatal;
 * the remaining tokens are still processed.
 * An empty string or NULL clears all three skip flags.
 */
void
parse_skip_transactions(const char *s, struct be_config *cfg)
{
	char	*buf, *p, *token;

	cfg->skip_install = false;
	cfg->skip_upgrade = false;
	cfg->skip_deinstall = false;

	if (s == NULL || *s == '\0')
		return;

	/*
	 * strsep() modifies its target in-place, so work on a heap copy.
	 * The value is at most a short comma-separated list; any failure to
	 * allocate is treated as an empty string (all flags stay false).
	 */
	if ((buf = strdup(s)) == NULL) {
		pkg_emit_notice("be-plugin: out of memory parsing "
		    "BE_PLUGIN_SKIP_TRANSACTIONS");
		return;
	}

	p = buf;
	while ((token = strsep(&p, ",")) != NULL) {
		char	*end;

		/*
		 * Strip leading and trailing whitespace so that both
		 * "install, upgrade" and "install ,upgrade" work.
		 */
		while (*token == ' ' || *token == '\t')
			token++;
		end = token + strlen(token);
		while (end > token && (end[-1] == ' ' || end[-1] == '\t'))
			*--end = '\0';

		if (*token == '\0')
			continue;

		if (strcmp(token, "install") == 0)
			cfg->skip_install = true;
		else if (strcmp(token, "upgrade") == 0)
			cfg->skip_upgrade = true;
		else if (strcmp(token, "deinstall") == 0)
			cfg->skip_deinstall = true;
		else
			pkg_emit_notice("be-plugin: unknown transaction type "
			    "\"%s\" in BE_PLUGIN_SKIP_TRANSACTIONS, ignoring",
			    token);
	}

	free(buf);
}

/*
 * config_register_keys -- declare plugin config keys and their defaults.
 *
 * Must be called before pkg_plugin_parse().  Each key registered here will
 * appear in the config object returned by pkg_plugin_conf() regardless of
 * whether the user has a config file.
 *
 * Returns EPKG_OK or EPKG_FATAL.
 */
int
config_register_keys(struct pkg_plugin *p)
{
	if (pkg_plugin_conf_add(p, PKG_BOOL, "BE_PLUGIN_ENABLED",
	    DEFAULT_ENABLED) != EPKG_OK)
		goto fail;
	if (pkg_plugin_conf_add(p, PKG_INT, "BE_PLUGIN_KEEP",
	    DEFAULT_KEEP) != EPKG_OK)
		goto fail;
	if (pkg_plugin_conf_add(p, PKG_STRING, "BE_PLUGIN_NAME_PREFIX",
	    DEFAULT_PREFIX) != EPKG_OK)
		goto fail;
	if (pkg_plugin_conf_add(p, PKG_STRING, "BE_PLUGIN_MIN_AGE",
	    DEFAULT_MIN_AGE) != EPKG_OK)
		goto fail;
	if (pkg_plugin_conf_add(p, PKG_BOOL, "BE_PLUGIN_STRICT",
	    DEFAULT_STRICT) != EPKG_OK)
		goto fail;
	if (pkg_plugin_conf_add(p, PKG_STRING, "BE_PLUGIN_SKIP_TRANSACTIONS",
	    DEFAULT_SKIP) != EPKG_OK)
		goto fail;
	if (pkg_plugin_conf_add(p, PKG_STRING, "BE_PLUGIN_REPOSITORIES",
	    DEFAULT_REPOSITORIES) != EPKG_OK)
		goto fail;

	return (EPKG_OK);

fail:
	pkg_plugin_error(p, "failed to register configuration keys");
	return (EPKG_FATAL);
}

/*
 * config_load -- read the parsed config object into *cfg.
 *
 * Sets compiled-in defaults first, then overwrites each field with the
 * value from the pkg config object.  This means any key absent from the
 * config object (because the user omitted it) retains its default.
 *
 * Call after pkg_plugin_parse() so that user-supplied values have been
 * merged into the object.
 *
 * Returns EPKG_OK or EPKG_FATAL on invalid config.
 */
int
config_load(struct pkg_plugin *p, struct be_config *cfg)
{
	const pkg_object	*conf, *obj;
	const char		*key, *val;
	pkg_iter		 it = NULL;

	/*
	 * Initialise to defaults first.  If the config object is NULL or
	 * missing a key, the default value stands.
	 */
	cfg->enabled = true;
	cfg->keep = 5;
	cfg->strict = false;
	(void)strlcpy(cfg->prefix, DEFAULT_PREFIX, sizeof(cfg->prefix));

	/*
	 * DEFAULT_MIN_AGE is a string constant; pass it through parse_duration
	 * so that defaults and user values go through the same code path.
	 * Failure here is a programming error in this file, not a user error.
	 */
	if (parse_duration(DEFAULT_MIN_AGE, &cfg->min_age) != 0) {
		pkg_plugin_error(p,
		    "internal error: DEFAULT_MIN_AGE \"%s\" is invalid",
		    DEFAULT_MIN_AGE);
		return (EPKG_FATAL);
	}

	parse_skip_transactions(DEFAULT_SKIP, cfg);
	cfg->repositories[0] = '\0';

	conf = pkg_plugin_conf(p);
	if (conf == NULL) {
		/*
		 * No config object: either pkg_plugin_conf_add was not called
		 * or something went wrong internally.  Use defaults and
		 * continue; the plugin is still functional.
		 */
		return (EPKG_OK);
	}

	while ((obj = pkg_object_iterate(conf, &it)) != NULL) {
		key = pkg_object_key(obj);
		if (key == NULL)
			continue;

		if (strcmp(key, "BE_PLUGIN_ENABLED") == 0) {
			cfg->enabled = pkg_object_bool(obj);

		} else if (strcmp(key, "BE_PLUGIN_KEEP") == 0) {
			cfg->keep = pkg_object_int(obj);
			if (cfg->keep < 1) {
				pkg_plugin_error(p,
				    "BE_PLUGIN_KEEP must be >= 1 (got %lld)",
				    (long long)cfg->keep);
				return (EPKG_FATAL);
			}

		} else if (strcmp(key, "BE_PLUGIN_NAME_PREFIX") == 0) {
			val = pkg_object_string(obj);
			if (val == NULL || *val == '\0') {
				pkg_plugin_error(p,
				    "BE_PLUGIN_NAME_PREFIX must not be empty");
				return (EPKG_FATAL);
			}
			/*
			 * Reject an over-length prefix outright rather than
			 * silently truncating it: a truncated prefix would
			 * produce BE names the user never intended, and prune
			 * matching would then key off that surprise prefix.
			 * strlcpy() returns the length of the source string.
			 */
			if (strlcpy(cfg->prefix, val, sizeof(cfg->prefix)) >=
			    sizeof(cfg->prefix)) {
				pkg_plugin_error(p,
				    "BE_PLUGIN_NAME_PREFIX is too long "
				    "(max %zu characters)",
				    sizeof(cfg->prefix) - 1);
				return (EPKG_FATAL);
			}

		} else if (strcmp(key, "BE_PLUGIN_MIN_AGE") == 0) {
			val = pkg_object_string(obj);
			if (parse_duration(val, &cfg->min_age) != 0) {
				pkg_plugin_error(p,
				    "BE_PLUGIN_MIN_AGE: invalid duration "
				    "\"%s\" (use e.g. \"7d\", \"24h\", "
				    "\"30m\", \"300s\", \"0\")",
				    val != NULL ? val : "(null)");
				return (EPKG_FATAL);
			}

		} else if (strcmp(key, "BE_PLUGIN_STRICT") == 0) {
			cfg->strict = pkg_object_bool(obj);

		} else if (strcmp(key, "BE_PLUGIN_SKIP_TRANSACTIONS") == 0) {
			val = pkg_object_string(obj);
			parse_skip_transactions(val != NULL ? val : "", cfg);

		} else if (strcmp(key, "BE_PLUGIN_REPOSITORIES") == 0) {
			val = pkg_object_string(obj);
			if (val != NULL)
				(void)strlcpy(cfg->repositories, val,
				    sizeof(cfg->repositories));
		}
	}

	return (EPKG_OK);
}
