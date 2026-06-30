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

#ifndef BE_NAMING_H
#define	BE_NAMING_H

#include <stdbool.h>
#include <stddef.h>

/*
 * be_naming.h -- libbe-free seam for boot-environment name disambiguation.
 *
 * The collision-resolution logic in be_disambiguate_name() depends on libbe
 * only through a single question: "does a BE with this exact name already
 * exist?"  That question is abstracted behind be_name_taken_fn so the logic
 * can be compiled into be.so (where the callback wraps be_exists()) and into
 * the test binary (where the callback consults an in-memory fixture) from the
 * same source -- no duplicated algorithm, no libbe or ZFS dependency in tests.
 */

/*
 * be_name_taken_fn -- predicate: is a BE with this exact name already taken?
 *
 * ctx is an opaque pass-through value: the libbe handle in production, a test
 * fixture in unit tests.  Returns true when the name is already in use.
 */
typedef bool (*be_name_taken_fn)(void *ctx, const char *name);

/*
 * be_disambiguate_name -- rewrite buf to a name the predicate reports as free.
 *
 * If buf is already free, it is left unchanged.  Otherwise "-N" (N from 2) is
 * appended to the original base name until taken() returns false, trying N up
 * to 99.  If every candidate through 99 is taken, buf is left holding the
 * "-99" candidate so the caller still surfaces the collision through its own
 * create path.
 *
 * Returns true if buf names a free BE on return, false if 2..99 were all
 * taken (buf then holds the last, still-taken candidate).
 */
bool
be_disambiguate_name(be_name_taken_fn taken, void *ctx,
    char *buf, size_t bufsz);

#endif				/* BE_NAMING_H */
