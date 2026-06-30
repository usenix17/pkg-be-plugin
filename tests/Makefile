# Tests for pkg-be-plugin config parsing and pruning helpers.
#
# test_config: parse_duration() and parse_skip_transactions() -- pure string
#   manipulation, no libbe or ZFS dependency.
#
# test_prune: cand_sort(), be_name_matches_prefix(), and be_prune_select() --
#   pure sort/string/selection logic from prune_testable.c, no libbe or ZFS
#   dependency.
#
# test_naming: be_disambiguate_name() from be_naming.c -- same-second BE name
#   collision handling, with be_exists() replaced by an in-memory fixture.
#
# prune_old_bes() and the libbe orchestration around these helpers require a
# live ZFS boot environment and are not covered by these unit tests.

.include <bsd.own.mk>

LOCALBASE?=	/usr/local

ATF_TESTS_C=	test_config test_prune test_naming

SRCS.test_config=	test_config.c \
			${.CURDIR}/../config.c

SRCS.test_prune=	test_prune.c \
			${.CURDIR}/../prune_testable.c

SRCS.test_naming=	test_naming.c \
			${.CURDIR}/../be_naming.c

# config.c and prune_testable.c live in the parent directory; bsd.dep.mk
# rejects absolute-path objects by default.
_ALLOW_ABSOLUTE_OBJ_PATH=	yes

CFLAGS+=	-I${.CURDIR}/.. \
		-I${LOCALBASE}/include

# All pkg symbols referenced by config.c are stubbed in test_config.c.
# prune_testable.c has no pkg or libbe dependency at all.
# Neither test binary needs libpkg or libe linkage.
LDADD.test_config+=	-lprivateatf-c
LDADD.test_prune+=	-lprivateatf-c
LDADD.test_naming+=	-lprivateatf-c

TESTSDIR=	/usr/tests/pkg-be-plugin

.include <bsd.test.mk>
