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

#ifndef PKG_BE_PLUGIN_H
#define	PKG_BE_PLUGIN_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * Runtime configuration populated from the plugin's UCL config file at init
 * time.  Written once in pkg_plugin_init(); thereafter treated as read-only.
 * All three .c files share this struct through this header.
 */
struct be_config {
	bool		enabled;	/* BE_PLUGIN_ENABLED */
	int64_t		keep;		/* BE_PLUGIN_KEEP: target max retained BEs */
	time_t		min_age;	/* BE_PLUGIN_MIN_AGE: converted to seconds */
	char		prefix[64];	/* BE_PLUGIN_NAME_PREFIX */
	bool		strict;		/* BE_PLUGIN_STRICT: abort on failure */
	bool		skip_install;	/* \                                    */
	bool		skip_upgrade;	/*  > derived from BE_PLUGIN_SKIP_TRANSACTIONS */
	bool		skip_deinstall;	/* /                                    */
};

/*
 * Module-level singletons, defined in pkg-be-plugin.c and referenced by
 * config.c and prune.c only where documented below.
 *
 * g_plugin: used by config.c to call pkg_plugin_error() / pkg_plugin_info()
 *           during config_load().
 * g_config: used by prune.c to be passed into prune_old_bes() by the caller
 *           rather than by direct access; declared here for completeness.
 */
struct pkg_plugin;			/* forward declaration; include <pkg.h> for full type */
extern struct pkg_plugin	*g_plugin;
extern struct be_config		 g_config;

/*
 * Plugin entry points called by pkg(8) via dlsym().  Declared here so that
 * the compiler can enforce the prototype when the definitions appear in
 * pkg-be-plugin.c without triggering -Wmissing-prototypes.
 */
int	pkg_plugin_init(struct pkg_plugin *);
int	pkg_plugin_shutdown(struct pkg_plugin *);

#endif	/* PKG_BE_PLUGIN_H */
