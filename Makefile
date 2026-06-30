# pkg-be-plugin: auto-create ZFS boot environments before pkg transactions
# Builds an unversioned shared object for loading via pkg(8)'s plugin system.

.include <bsd.own.mk>

LOCALBASE?=     /usr/local

SHLIB_NAME=     be.so
NO_SHLIB_LINKS= yes

SRCS=           pkg-be-plugin.c \
	        config.c \
	        prune.c \
	        prune_testable.c \
	        be_naming.c

WARNS?=         6

CFLAGS+=        -I${LOCALBASE}/include
LDFLAGS+=       -L${LOCALBASE}/lib
LDADD+=         -lbe -lpkg

# Install directly into the pkg plugin directory, not the default lib dir.
SHLIBDIR=       ${LOCALBASE}/lib/pkg

MAN=            pkg-be-plugin.8
MANDIR=         ${LOCALBASE}/share/man/man

# Create the install destinations ourselves.  bsd.dirs.mk's DIRS takes variable
# *names* (it installs ${${dir}}), so "DIRS+= /path" silently expands to nothing
# and the directory is never made; and even spelled correctly its installdirs
# target is not ordered ahead of _libinstall/maninstall.  beforeinstall is, per
# the .ORDER lines in bsd.lib.mk, guaranteed to run first.  ${MANDIR}8 is the
# man8 section dir.  The .debug subdir for the separated debug file is created
# by bsd.lib.mk's own _debuginstall, so it is not made here.
beforeinstall:
	${INSTALL} -d -m 0755 ${DESTDIR}${SHLIBDIR}
	${INSTALL} -d -m 0755 ${DESTDIR}${MANDIR}8

# Files to clean beyond bsd.lib.mk's defaults
CLEANFILES+=    *.pico *.pieo *.so *.so.debug *.so.full
CLEANFILES+=    pkg-be-plugin.8.gz
CLEANFILES+=    .depend.*
CLEANFILES+=    *.fmt

# Files subject to indent(1) formatting (includes test helpers not in SRCS)
FMT_FILES=      ${SRCS} \
	        pkg-be-plugin.h config.h prune.h be_compat.h \
	        prune_testable.h be_naming.h

# Project and library typedefs.  Without -T, indent does not know these names
# are types and misparses declarations that use them -- collapsing the brace of
# a function whose last parameter is one of them, and adding a stray space to
# casts like (int64_t)x.  Listing them keeps the formatter's output correct.
INDENT_TYPES=   -Tlibbe_handle_t -Tnvlist_t -Tnvpair_t -Tpkg_jobs_t \
	        -Tpkg_object -Tpkg_iter -Tbe_name_taken_fn -Tbe_destroy_fn \
	        -Tbe_defer_fn -Tint64_t -Ttime_t -Tsize_t -Tuint_t -Tuchar_t \
	        -Tboolean_t -Thrtime_t -Tva_list

# -di16 column-aligns declaration names (KNF); -nfc1/-nfcb leave hand-formatted
# block comments (bullet lists, ASCII layout) untouched; -nsob preserves the
# intentional blank lines that -sob would otherwise swallow.
INDENT_FLAGS=   -nbad -bap -nbc -br -ce -ci4 -cli0 -d0 -di16 -i8 -ip -l79 \
	        -nlp -npcs -psl -sc -nsob -nfc1 -nfcb ${INDENT_TYPES}

# indent(1) accepts at most an input and an output file, so format one file at
# a time through a temp file (in place, no .BAK clutter).  The test -s guard
# means a hiccup that yields empty output can never clobber the source file.
fmt:
	@for f in ${FMT_FILES}; do \
	        indent ${INDENT_FLAGS} < $$f > $$f.fmt; \
	        if test -s $$f.fmt; then \
	                mv $$f.fmt $$f; echo "formatted $$f"; \
	        else \
	                rm -f $$f.fmt; \
	                echo "indent produced no output for $$f" >&2; exit 1; \
	        fi; \
	done

fmt-check:
	@for f in ${FMT_FILES}; do \
	        indent ${INDENT_FLAGS} < $$f | diff -u $$f - || \
	                { echo "Formatting differs in $$f"; exit 1; }; \
	done
	@echo "All files match KNF format"

test:
	cd ${.CURDIR}/tests && ${MAKE}

.include <bsd.lib.mk>
