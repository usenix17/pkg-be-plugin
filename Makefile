# pkg-be-plugin: auto-create ZFS boot environments before pkg transactions
# Builds an unversioned shared object for loading via pkg(8)'s plugin system.

.include <bsd.own.mk>

LOCALBASE?=     /usr/local

SHLIB_NAME=     be.so
NO_SHLIB_LINKS= yes

SRCS=           pkg-be-plugin.c \
	        config.c \
	        prune.c

WARNS?=         6

CFLAGS+=        -I${LOCALBASE}/include
LDFLAGS+=       -L${LOCALBASE}/lib
LDADD+=         -lbe -lpkg

# Install directly into the pkg plugin directory, not the default lib dir.
SHLIBDIR=       ${LOCALBASE}/lib/pkg

MAN=            pkg-be-plugin.8
MANDIR=         ${LOCALBASE}/share/man/man

DIRS+=          ${LOCALBASE}/lib/pkg
DIRS+=          ${LOCALBASE}/share/man/man8

# Files to clean beyond bsd.lib.mk's defaults
CLEANFILES+=    *.pico *.pieo *.so *.so.debug *.so.full
CLEANFILES+=    pkg-be-plugin.8.gz
CLEANFILES+=    .depend.*

# Files subject to indent(1) formatting (includes test helpers not in SRCS)
FMT_FILES=      ${SRCS} prune_testable.c \
	        pkg-be-plugin.h config.h prune.h be_compat.h prune_testable.h

INDENT_FLAGS=   -nbad -bap -nbc -br -ce -ci4 -cli0 -d0 -di0 -i8 -ip -l79 \
	        -nlp -npcs -psl -sc -sob

fmt:
	indent ${INDENT_FLAGS} ${FMT_FILES}

fmt-check:
	@for f in ${FMT_FILES}; do \
	        indent ${INDENT_FLAGS} < $$f | diff -u $$f - || \
	                { echo "Formatting differs in $$f"; exit 1; }; \
	done
	@echo "All files match KNF format"

test:
	cd ${.CURDIR}/tests && ${MAKE}

.include <bsd.lib.mk>
