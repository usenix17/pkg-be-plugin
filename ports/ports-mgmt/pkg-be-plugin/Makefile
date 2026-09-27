PORTNAME=	pkg-be-plugin
DISTVERSION=	1.0
CATEGORIES=	ports-mgmt sysutils

MAINTAINER=	sasha@starnix.net
COMMENT=	Pkg(8) plugin to auto-create ZFS boot environments
WWW=		https://github.com/usenix17/pkg-be-plugin

LICENSE=	BSD2CLAUSE
LICENSE_FILE=	${WRKSRC}/LICENSE

LIB_DEPENDS=	libpkg.so:ports-mgmt/pkg

USES=		uidfix
USE_GITHUB=	yes
GH_ACCOUNT=	usenix17
GH_TAGNAME=	${DISTVERSION}

SUB_FILES=	pkg-message

PLIST_FILES=	lib/pkg/be.so \
		"@sample etc/pkg/be.conf.sample" \
		share/man/man8/pkg-be-plugin.8.gz

OPTIONS_DEFINE=	DOCS
PORTDOCS=	README.md design.md

pre-install:
	@${MKDIR} ${STAGEDIR}${PREFIX}/lib/pkg
	@${MKDIR} ${STAGEDIR}${PREFIX}/etc/pkg
	@${MKDIR} ${STAGEDIR}${PREFIX}/share/man/man8

post-install:
	${INSTALL_DATA} ${WRKSRC}/be.conf \
		${STAGEDIR}${PREFIX}/etc/pkg/be.conf.sample
	${STRIP_CMD} ${STAGEDIR}${PREFIX}/lib/pkg/be.so

post-install-DOCS-on:
	@${MKDIR} ${STAGEDIR}${DOCSDIR}
	${INSTALL_DATA} ${WRKSRC}/README.md ${STAGEDIR}${DOCSDIR}
	${INSTALL_DATA} ${WRKSRC}/design.md ${STAGEDIR}${DOCSDIR}

.include <bsd.port.mk>
