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

#ifndef BE_COMPAT_H
#define	BE_COMPAT_H

/*
 * be.h includes libnvpair.h which includes sys/nvpair.h.  That header is
 * derived from OpenSolaris/OpenZFS and uses several types that FreeBSD only
 * defines in kernel-mode headers (_KERNEL-gated in sys/types.h):
 *
 *   uint_t    -- unsigned int   (OpenSolaris/Solaris native uint)
 *   boolean_t -- unsigned int   (matches FreeBSD kernel def in sys/types.h)
 *   uchar_t   -- unsigned char  (Solaris uchar)
 *   hrtime_t  -- long long      (high-resolution timer, nanoseconds)
 *
 * Define them here for user-space compilation.  C17 (the standard we compile
 * under) permits compatible redeclarations of typedefs within a translation
 * unit, so these are safe even if a future include path also defines them
 * identically.
 *
 * All files that need be.h must include this header instead.
 */

typedef unsigned int uint_t;
typedef unsigned int boolean_t;
typedef unsigned char uchar_t;
typedef long long hrtime_t;

/*
 * sys/nvpair.h uses va_list in nv_alloc_ops.  Include stdarg.h here so
 * that be_compat.h is self-contained regardless of what the including
 * translation unit has already included.
 */
#include <stdarg.h>

#include <be.h>

#endif				/* BE_COMPAT_H */
