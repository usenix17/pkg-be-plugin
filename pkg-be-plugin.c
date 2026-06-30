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
 * pkg-be-plugin.c -- pkg(8) plugin entry points and hook dispatch.
 *
 * This file owns:
 *   - pkg_plugin_init() and pkg_plugin_shutdown() (exported entry points)
 *   - be_hook() (single callback for all three pre-transaction hooks)
 *   - generate_be_name() (formats the timestamped BE name)
 *   - be_hook_name() (maps pkg_jobs_t to a log-friendly string)
 *   - g_plugin and g_config (module-level singletons)
 *
 * libbe is called directly here (be_create, libbe_init/close, etc.).
 * All pruning-related libbe calls are in prune.c.
 */

#include <sys/cdefs.h>

#include <time.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>

#include "be_compat.h"
#include <pkg.h>

#include "pkg-be-plugin.h"
#include "be_naming.h"
#include "config.h"
#include "prune.h"

/*
 * BE_NAME_LEN: maximum buffer size for a generated BE name.
 *
 * A name is: prefix (up to 63 chars) + "-" + "YYYYMMDD" + "-" + "HHMMSS"
 * That is at most 63 + 1 + 8 + 1 + 6 = 79 characters.  128 is generous.
 */
#define	BE_NAME_LEN	128

struct pkg_plugin *g_plugin;
struct be_config g_config;

/*
 * be_hook_name -- return a human-readable label for a transaction type.
 *
 * Used in syslog and pkg error messages so that a single hook function
 * produces descriptive output for all three registered transaction types.
 */
static const char *
be_hook_name(pkg_jobs_t type)
{
	switch (type) {
	case PKG_JOBS_INSTALL:
		return ("pre-install");
	case PKG_JOBS_UPGRADE:
		return ("pre-upgrade");
	case PKG_JOBS_DEINSTALL:
		return ("pre-deinstall");
	default:
		return ("unknown");
	}
}

/*
 * generate_be_name -- format a timestamped boot environment name.
 *
 * Produces: "<prefix>-YYYYMMDD-HHMMSS", e.g. "pre-pkg-20260513-142301".
 * Uses local time.  buf must be at least BE_NAME_LEN bytes.
 */
static void
generate_be_name(const char *prefix, char *buf, size_t bufsz)
{
	time_t		now;
	struct tm	tm;

	time(&now);
	localtime_r(&now, &tm);
	(void)snprintf(buf, bufsz, "%s-%04d%02d%02d-%02d%02d%02d",
	    prefix,
	    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/*
 * be_name_taken_libbe -- be_name_taken_fn backed by libbe's be_exists().
 *
 * be_exists() returns BE_ERR_SUCCESS (0) when the BE exists.  ctx is the
 * libbe handle.  This is the production half of the be_naming.c seam.
 */
static bool
be_name_taken_libbe(void *ctx, const char *name)
{
	libbe_handle_t *hdl = ctx;

	return (be_exists(hdl, name) == BE_ERR_SUCCESS);
}

/*
 * disambiguate_be_name -- ensure buf names a BE that does not already exist.
 *
 * generate_be_name() has one-second resolution, so two transactions in the
 * same wall-clock second (scripted runs, CI) would generate identical names
 * and the second be_create() would fail with "already exists" -- which, in
 * strict mode, aborts an otherwise valid transaction.  The collision-handling
 * logic lives in be_disambiguate_name() (be_naming.c) so it can be unit
 * tested without libbe; here we just bind it to be_exists().  If 2..99 are all
 * taken we leave the last candidate in place and let be_create() report the
 * collision through the normal error path.
 */
static void
disambiguate_be_name(libbe_handle_t *hdl, char *buf, size_t bufsz)
{
	(void)be_disambiguate_name(be_name_taken_libbe, hdl, buf, bufsz);
}

/*
 * repo_matches -- return true if any package in the job set originates from
 * a repository named in the comma-separated filter string.
 *
 * Comparison is case-sensitive; token whitespace is stripped.  The special
 * token "local" matches packages with no repository (installed via pkg add).
 * If filter is empty or NULL, every transaction matches (no filtering).
 */
static bool
repo_matches(struct pkg_jobs *jobs, const char *filter)
{
	struct pkg     *new_pkg, *old_pkg;
	void	       *iter;
	int		jtype;
	const char     *reponame;
	char		buf[512], *p, *token;
	bool		matched;

	if (filter == NULL || *filter == '\0')
		return (true);

	iter = NULL;
	while (pkg_jobs_iter(jobs, &iter, &new_pkg, &old_pkg, &jtype)) {
		struct pkg     *pkg = new_pkg != NULL ? new_pkg : old_pkg;

		if (pkg == NULL)
			continue;

		reponame = NULL;
		pkg_get(pkg, PKG_ATTR_REPONAME, &reponame);

		/*
		 * Work through the filter list for this package.  We re-parse
		 * the filter string for each package; it is short and this path
		 * runs once per transaction.
		 */
		(void)strlcpy(buf, filter, sizeof(buf));
		p = buf;
		matched = false;
		while ((token = strsep(&p, ",")) != NULL && !matched) {
			char	       *end;

			/*
			 * Strip leading and trailing whitespace so that
			 * "repo1 , repo2" matches both tokens.
			 */
			while (*token == ' ' || *token == '\t')
				token++;
			end = token + strlen(token);
			while (end > token &&
			    (end[-1] == ' ' || end[-1] == '\t'))
				*--end = '\0';
			if (*token == '\0')
				continue;

			if (strcmp(token, "local") == 0) {
				if (reponame == NULL || *reponame == '\0')
					matched = true;
			} else {
				if (reponame != NULL &&
				    strcmp(reponame, token) == 0)
					matched = true;
			}
		}

		if (matched)
			return (true);
	}

	return (false);
}

/*
 * be_hook -- pre-transaction hook callback.
 *
 * Registered for PKG_PLUGIN_HOOK_PRE_INSTALL, PKG_PLUGIN_HOOK_PRE_UPGRADE,
 * and PKG_PLUGIN_HOOK_PRE_DEINSTALL.  For all three hooks, pkg passes a
 * struct pkg_jobs * as data.
 *
 * Error handling follows the taxonomy in design.md:
 *   - success:             syslog LOG_NOTICE + pkg_emit_notice
 *   - failure, non-strict: syslog LOG_WARNING + pkg_plugin_error, return OK
 *   - failure, strict:     syslog LOG_ERR + pkg_plugin_error, return FATAL
 *
 * On the success path the libbe handle is closed before prune_old_bes() is
 * called so the pruner can open its own handle cleanly.  On error paths the
 * done: label closes it.
 */
static int
be_hook(void *data, struct pkgdb *db)
{
	libbe_handle_t *hdl;
	struct pkg_jobs *jobs;
	pkg_jobs_t	type;
	const char     *hook_name;
	char		be_name[BE_NAME_LEN];
	int		error;

	(void)db;

	if (data == NULL)
		return (EPKG_OK);

	jobs = data;
	type = pkg_jobs_type(jobs);
	hook_name = be_hook_name(type);

	switch (type) {
	case PKG_JOBS_INSTALL:
		if (g_config.skip_install)
			return (EPKG_OK);
		break;
	case PKG_JOBS_UPGRADE:
		if (g_config.skip_upgrade)
			return (EPKG_OK);
		break;
	case PKG_JOBS_DEINSTALL:
		if (g_config.skip_deinstall)
			return (EPKG_OK);
		break;
	default:
		/*
		 * pkg fired a hook type we were not registered for.  This
		 * should not happen, but handle it cleanly.
		 */
		pkg_plugin_info(g_plugin,
		    "unhandled jobs type %d, skipping", (int)type);
		return (EPKG_OK);
	}

	/*
	 * Repository filter: if BE_PLUGIN_REPOSITORIES is set, skip the
	 * transaction unless at least one package comes from a listed repo.
	 */
	if (!repo_matches(jobs, g_config.repositories))
		return (EPKG_OK);

	generate_be_name(g_config.prefix, be_name, sizeof(be_name));

	hdl = NULL;
	error = 0;

	if ((hdl = libbe_init(NULL)) == NULL) {
		/*
		 * libbe_init() fails when the system is not booted from a ZFS
		 * boot environment (e.g. UFS root, or a restricted jail).
		 * This is the most common runtime failure in non-ZFS-BE setups.
		 */
		syslog(LOG_WARNING,
		    "pkg-be-plugin: %s: libbe_init failed: "
		    "not a ZFS boot environment system", hook_name);
		pkg_plugin_error(g_plugin,
		    "%s: libbe_init failed: not a ZFS boot environment system",
		    hook_name);
		error = 1;
		goto done;
	}

	/*
	 * Suppress libbe's own stderr output; we feed errors through
	 * pkg_plugin_error() and syslog instead.
	 */
	libbe_print_on_error(hdl, false);

	/*
	 * be_validate_name() checks character set and length constraints.
	 * It does NOT set the internal libbe error state, so
	 * libbe_error_description() is not useful here.
	 */
	if (be_validate_name(hdl, be_name) != BE_ERR_SUCCESS) {
		syslog(LOG_WARNING,
		    "pkg-be-plugin: %s: invalid BE name \"%s\"",
		    hook_name, be_name);
		pkg_plugin_error(g_plugin,
		    "%s: invalid BE name \"%s\" -- check BE_PLUGIN_NAME_PREFIX",
		    hook_name, be_name);
		error = 1;
		goto done;
	}

	/*
	 * Resolve same-second name collisions before creating.  The base name
	 * already passed be_validate_name(); the "-N" suffix only adds a hyphen
	 * and digits, so the result stays valid and well under the length cap.
	 */
	disambiguate_be_name(hdl, be_name, sizeof(be_name));

	if (be_create(hdl, be_name) != BE_ERR_SUCCESS) {
		syslog(LOG_WARNING,
		    "pkg-be-plugin: %s: be_create(\"%s\") failed: %s",
		    hook_name, be_name, libbe_error_description(hdl));
		pkg_plugin_error(g_plugin,
		    "%s: failed to create boot environment \"%s\": %s",
		    hook_name, be_name, libbe_error_description(hdl));
		error = 1;
		goto done;
	}

	/*
	 * Success.  Close the libbe handle before pruning so that
	 * prune_old_bes() can open its own clean handle without any
	 * concurrent handle from this side interfering.
	 */
	syslog(LOG_NOTICE,
	    "pkg-be-plugin: created boot environment \"%s\"", be_name);
	pkg_emit_notice("be-plugin: created boot environment: %s", be_name);

	libbe_close(hdl);
	hdl = NULL;

	prune_old_bes(g_config.prefix, g_config.keep, g_config.min_age);

done:
	if (hdl != NULL)
		libbe_close(hdl);

	if (error && g_config.strict) {
		syslog(LOG_ERR,
		    "pkg-be-plugin: aborting %s transaction "
		    "(strict mode, BE creation failed)", hook_name);
		return (EPKG_FATAL);
	}
	return (EPKG_OK);
}

/*
 * pkg_plugin_init -- plugin entry point called by pkg(8) at load time.
 *
 * Sets metadata, registers config keys, parses the config file, loads
 * config into g_config, and registers the pre-transaction hooks.
 *
 * If BE_PLUGIN_ENABLED is false, hooks are not registered and the plugin
 * loads but is inert.
 */
int
pkg_plugin_init(struct pkg_plugin *p)
{
	g_plugin = p;

	pkg_plugin_set(p, PKG_PLUGIN_NAME, "be");
	pkg_plugin_set(p, PKG_PLUGIN_DESC,
	    "Automatically create ZFS boot environments before pkg transactions");
	pkg_plugin_set(p, PKG_PLUGIN_VERSION, "1.0.0");

	if (config_register_keys(p) != EPKG_OK)
		return (EPKG_FATAL);

	/*
	 * pkg_plugin_parse() reads PLUGINS_CONF_DIR/be.conf and merges any
	 * user-supplied values with the defaults registered above.
	 * A missing config file is not an error; defaults stand.
	 */
	if (pkg_plugin_parse(p) != EPKG_OK) {
		pkg_plugin_error(p, "failed to parse configuration file");
		return (EPKG_FATAL);
	}

	if (config_load(p, &g_config) != EPKG_OK)
		return (EPKG_FATAL);

	if (!g_config.enabled)
		return (EPKG_OK);

	/*
	 * Probe for ZFS boot-environment support once, at load time.  On
	 * systems without a ZFS BE (UFS root, jails without ZFS access)
	 * libbe_init() returns NULL.  Rather than registering hooks that then
	 * fail -- and emit a pkg error -- on every single transaction (and, in
	 * strict mode, abort every transaction), leave the plugin inert: emit a
	 * single informational message and register no hooks.
	 */
	{
		libbe_handle_t *probe;

		if ((probe = libbe_init(NULL)) == NULL) {
			pkg_plugin_info(p,
			    "not a ZFS boot environment system; "
			    "boot environments will not be created");
			return (EPKG_OK);
		}
		libbe_close(probe);
	}

	if (pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_PRE_INSTALL,
	    be_hook) != EPKG_OK) {
		pkg_plugin_error(p, "failed to register pre-install hook");
		return (EPKG_FATAL);
	}
	if (pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_PRE_UPGRADE,
	    be_hook) != EPKG_OK) {
		pkg_plugin_error(p, "failed to register pre-upgrade hook");
		return (EPKG_FATAL);
	}
	if (pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_PRE_DEINSTALL,
	    be_hook) != EPKG_OK) {
		pkg_plugin_error(p, "failed to register pre-deinstall hook");
		return (EPKG_FATAL);
	}

	/*
	 * openlog() stores its ident argument as a pointer into our .so's
	 * text segment -- not a copy.  Call it only after all error-return
	 * paths so that pkg_plugin_shutdown() (which calls closelog()) is
	 * guaranteed to run before the library is dlclose()'d.  Calling
	 * openlog() on an early error-return path and then returning
	 * EPKG_FATAL causes pkg to skip shutdown and eventually dlclose the
	 * library, leaving syslog's internal LogTag pointing at unmapped
	 * memory; the next syslog call from anywhere in the process segfaults.
	 */
	openlog("pkg-be-plugin", LOG_PID, LOG_DAEMON);

	return (EPKG_OK);
}

/*
 * pkg_plugin_shutdown -- plugin entry point called by pkg(8) at unload time.
 *
 * g_config contains no heap-allocated members; nothing to free.
 */
int
pkg_plugin_shutdown(struct pkg_plugin *p)
{
	(void)p;
	closelog();
	return (EPKG_OK);
}
