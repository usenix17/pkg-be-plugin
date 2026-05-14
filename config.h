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

#ifndef CONFIG_H
#define	CONFIG_H

struct pkg_plugin;
struct be_config;

/*
 * config_register_keys: declare all plugin config keys and their defaults
 * via pkg_plugin_conf_add(), then call pkg_plugin_parse() to read the UCL
 * config file.  Must be called before config_load().
 * Returns EPKG_OK or EPKG_FATAL.
 */
int	config_register_keys(struct pkg_plugin *);

/*
 * config_load: walk the parsed config object and populate *cfg.
 * Validates values (e.g. parses min_age duration string).
 * Returns EPKG_OK or EPKG_FATAL on invalid config.
 */
int	config_load(struct pkg_plugin *, struct be_config *);

/*
 * parse_duration: convert a duration string ("7d", "24h", "30m", "300") to
 * seconds.  Exposed for unit testing.
 * Returns 0 on success, -1 on parse error.
 */
int	parse_duration(const char *, time_t *);

/*
 * parse_skip_transactions: parse a comma-separated list of transaction types
 * ("install", "upgrade", "deinstall") and set the corresponding bool fields
 * in *cfg.  Unknown tokens produce a warning but are not fatal.
 * Exposed for unit testing.
 */
void	parse_skip_transactions(const char *, struct be_config *);

#endif	/* CONFIG_H */
