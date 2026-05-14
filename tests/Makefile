# Tests for pkg-be-plugin config parsing and pruning helpers.
#
# test_config: parse_duration() and parse_skip_transactions() -- pure string
#   manipulation, no libbe or ZFS dependency.
#
# test_prune: cand_sort() and be_name_matches_prefix() -- pure sort/string
#   logic from prune_testable.c, no libbe or ZFS dependency.
#
# prune_old_bes() itself requires a live ZFS boot environment and is not
# covered by these unit tests.

.include <bsd.own.mk>

LOCALBASE?=	/usr/local

ATF_TESTS_C=	test_config test_prune

SRCS.test_config=	test_config.c \
			${.CURDIR}/../config.c

SRCS.test_prune=	test_prune.c \
			${.CURDIR}/../prune_testable.c

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

TESTSDIR=	/usr/tests/pkg-be-plugin

.include <bsd.test.mk>
