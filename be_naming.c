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
 * be_naming.c -- boot-environment name disambiguation, free of libbe.
 *
 * Compiled into be.so (via the production wrapper in pkg-be-plugin.c) and into
 * the test_naming binary.  See be_naming.h for the seam rationale.
 */

#include <sys/cdefs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "be_naming.h"

/*
 * BE_DISAMBIG_BASE_MAX: scratch storage for the original base name while the
 * "-N" suffixes are tried.  Comfortably larger than the 128-byte BE name
 * buffer the plugin generates; oversized input is truncated by strlcpy, which
 * is harmless because a truncated base only ever produces a still-unique name.
 */
#define	BE_DISAMBIG_BASE_MAX	256

bool
be_disambiguate_name(be_name_taken_fn taken, void *ctx,
    char *buf, size_t bufsz)
{
	char		base[BE_DISAMBIG_BASE_MAX];
	int		n;

	if (taken == NULL || buf == NULL || bufsz == 0)
		return (true);

	if (!taken(ctx, buf))
		return (true);

	(void)strlcpy(base, buf, sizeof(base));
	for (n = 2; n < 100; n++) {
		(void)snprintf(buf, bufsz, "%s-%d", base, n);
		if (!taken(ctx, buf))
			return (true);
	}

	return (false);
}
