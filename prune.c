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
 * prune.c -- boot environment pruning logic.
 *
 * prune_old_bes() enumerates BEs whose names start with "<prefix>-",
 * sorts them oldest-first by ZFS creation timestamp, then destroys the
 * oldest ones until at most keep remain.  BEs younger than min_age are
 * skipped with a one-time deferred notice.
 *
 * All failures are best-effort: logged to syslog, never fatal to the
 * pkg transaction.
 */

#include <sys/cdefs.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "be_compat.h"	/* defines uint_t et al., then #include <be.h> */
#include <pkg.h>

#include "pkg-be-plugin.h"
#include "prune.h"

/*
 * BE_CAND_NAME_MAX: storage for one BE name in the candidate array.
 * ZFS dataset component names are at most 256 bytes.
 */
#define	BE_CAND_NAME_MAX	256

/*
 * Candidate array sizing.  Initial capacity is small; the array doubles
 * on each overflow up to CAND_MAX_CAP.  Beyond that, excess BEs are
 * silently ignored (pathological case -- normal systems have < 50 BEs).
 */
#define	CAND_INIT_CAP	8
#define	CAND_MAX_CAP	4096

struct be_candidate {
	char	name[BE_CAND_NAME_MAX];
	time_t	creation;		/* ZFS "creation" property (Unix time) */
};

/*
 * cand_cmp -- qsort comparator: oldest creation timestamp first.
 */
static int
cand_cmp(const void *a, const void *b)
{
	const struct be_candidate	*ca = a;
	const struct be_candidate	*cb = b;

	if (ca->creation < cb->creation)
		return (-1);
	if (ca->creation > cb->creation)
		return (1);
	return (0);
}

/*
 * prune_old_bes -- enumerate and destroy excess boot environments.
 *
 * Algorithm:
 *   1. libbe_init() + be_get_bootenv_props() to enumerate all BEs.
 *   2. Collect those whose name starts with "<prefix>-" into a dynamic
 *      array, parsing the "creation" property (a decimal Unix epoch string
 *      returned by libbe) into a time_t for each.
 *   3. If count <= keep, nothing to do.
 *   4. qsort oldest-first.
 *   5. Mark the oldest (count - keep) for deletion.
 *   6. For each candidate: if age < min_age, emit one deferred notice
 *      and stop (remaining candidates are even newer).
 *      Otherwise call be_destroy(); log success or failure per-BE.
 *
 * be_destroy() will refuse to destroy the active or next-boot BE and
 * return an error; we log that and move on.
 */
void
prune_old_bes(const char *prefix, int64_t keep, time_t min_age)
{
	libbe_handle_t		*hdl;
	nvlist_t		*props;
	nvpair_t		*pair;
	nvlist_t		*be_props;
	const char		*name;
	const char		*creation_str;
	time_t			 creation;
	time_t			 now;
	struct be_candidate	*cands;
	struct be_candidate	*tmp;
	size_t			 n_cands, cap, newcap;
	size_t			 i, n_delete;
	bool			 deferred_notice;
	size_t			 prefix_len;

	if (prefix == NULL || *prefix == '\0' || keep < 1)
		return;

	prefix_len = strlen(prefix);

	hdl = libbe_init(NULL);
	if (hdl == NULL) {
		syslog(LOG_WARNING, "pkg-be-plugin: prune: libbe_init failed");
		return;
	}
	libbe_print_on_error(hdl, false);

	props = NULL;
	if (be_prop_list_alloc(&props) != 0) {
		syslog(LOG_WARNING,
		    "pkg-be-plugin: prune: be_prop_list_alloc failed");
		libbe_close(hdl);
		return;
	}

	if (be_get_bootenv_props(hdl, props) != BE_ERR_SUCCESS) {
		syslog(LOG_WARNING,
		    "pkg-be-plugin: prune: be_get_bootenv_props failed: %s",
		    libbe_error_description(hdl));
		be_prop_list_free(props);
		libbe_close(hdl);
		return;
	}

	cap = CAND_INIT_CAP;
	cands = malloc(cap * sizeof(*cands));
	if (cands == NULL) {
		syslog(LOG_WARNING, "pkg-be-plugin: prune: malloc failed");
		be_prop_list_free(props);
		libbe_close(hdl);
		return;
	}
	n_cands = 0;

	for (pair = nvlist_next_nvpair(props, NULL);
	    pair != NULL;
	    pair = nvlist_next_nvpair(props, pair)) {

		name = nvpair_name(pair);

		/*
		 * Accept only BEs whose name is exactly "<prefix>-..." --
		 * the prefix followed immediately by a dash.  This avoids
		 * matching a user BE named "pre-pkg-extra" if our prefix is
		 * "pre-pkg" (the dash-terminated match is sufficient because
		 * generate_be_name always appends "-YYYYMMDD-HHMMSS").
		 */
		if (strncmp(name, prefix, prefix_len) != 0 ||
		    name[prefix_len] != '-')
			continue;

		if (nvpair_value_nvlist(pair, &be_props) != 0) {
			syslog(LOG_WARNING,
			    "pkg-be-plugin: prune: skipped \"%s\" "
			    "(nvpair_value_nvlist failed)", name);
			continue;
		}

		if (nvlist_lookup_string(be_props, "creation",
		    &creation_str) != 0) {
			syslog(LOG_WARNING,
			    "pkg-be-plugin: prune: skipped \"%s\" "
			    "(creation property missing)", name);
			continue;
		}

		/*
		 * libbe stores the creation timestamp as a string of decimal
		 * digits representing a Unix epoch (e.g. "1778725572"),
		 * not a formatted date.  Parse with strtoll().
		 */
		{
			char		*endptr;
			long long	 val;

			errno = 0;
			val = strtoll(creation_str, &endptr, 10);
			if (errno != 0 || endptr == creation_str ||
			    *endptr != '\0' || val < 0) {
				syslog(LOG_WARNING,
				    "pkg-be-plugin: prune: skipped \"%s\" "
				    "(cannot parse creation \"%s\")",
				    name, creation_str);
				continue;
			}
			creation = (time_t)val;
		}

		/* Grow array if needed, capped at CAND_MAX_CAP. */
		if (n_cands == cap) {
			if (cap >= CAND_MAX_CAP) {
				/*
				 * Pathological: more matching BEs than we are
				 * willing to track.  Stop collecting and prune
				 * what we have.  Because the array is not yet
				 * sorted, this prunes by enumeration order rather
				 * than strictly oldest-first, but a system with
				 * this many BEs is already misconfigured.
				 */
				syslog(LOG_WARNING,
				    "pkg-be-plugin: prune: more than %d matching "
				    "BEs; examining only the first %d",
				    CAND_MAX_CAP, CAND_MAX_CAP);
				break;
			}
			newcap = cap * 2;
			if (newcap > CAND_MAX_CAP)
				newcap = CAND_MAX_CAP;
			tmp = realloc(cands, newcap * sizeof(*cands));
			if (tmp == NULL)
				break;		/* use what we have so far */
			cands = tmp;
			cap = newcap;
		}

		(void)strlcpy(cands[n_cands].name, name,
		    sizeof(cands[n_cands].name));
		cands[n_cands].creation = creation;
		n_cands++;
	}

	be_prop_list_free(props);

	syslog(LOG_NOTICE,
	    "pkg-be-plugin: prune: enumerated %zu BEs matching prefix", n_cands);

	if ((int64_t)n_cands <= keep) {
		free(cands);
		libbe_close(hdl);
		return;
	}

	qsort(cands, n_cands, sizeof(*cands), cand_cmp);

	now = time(NULL);
	n_delete = (size_t)((int64_t)n_cands - keep);
	deferred_notice = false;

	for (i = 0; i < n_delete; i++) {
		if (min_age > 0 && now - cands[i].creation < min_age) {
			/*
			 * This candidate is younger than min_age.  Because
			 * the array is sorted oldest-first, all remaining
			 * candidates are even younger -- stop here and
			 * emit one notice covering the whole remainder.
			 */
			if (!deferred_notice) {
				syslog(LOG_NOTICE,
				    "pkg-be-plugin: prune: %zu BE(s) over "
				    "keep=%lld limit but under min_age=%llds; "
				    "will prune later",
				    n_delete - i, (long long)keep,
				    (long long)min_age);
				pkg_emit_notice(
				    "be-plugin: %zu BE(s) over keep limit "
				    "but under min_age; will prune later",
				    n_delete - i);
				deferred_notice = true;
			}
			break;
		}

		/*
		 * BE_DESTROY_AUTOORIGIN also reclaims the origin snapshot that
		 * be_create() left behind when it cloned this BE.  Without it
		 * those snapshots accumulate under the active BE and consume
		 * pool space indefinitely.  AUTOORIGIN (rather than
		 * BE_DESTROY_ORIGIN) only removes the origin when libbe
		 * recognises it as auto-created, so a manually cloned BE is left
		 * untouched and the call never fails on a shared snapshot.
		 */
		if (be_destroy(hdl, cands[i].name, BE_DESTROY_AUTOORIGIN) !=
		    BE_ERR_SUCCESS) {
			syslog(LOG_WARNING,
			    "pkg-be-plugin: prune: be_destroy(\"%s\") "
			    "failed: %s",
			    cands[i].name, libbe_error_description(hdl));
		} else {
			syslog(LOG_NOTICE,
			    "pkg-be-plugin: prune: destroyed \"%s\"",
			    cands[i].name);
		}
	}

	free(cands);
	libbe_close(hdl);
}
