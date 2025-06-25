#	$OpenBSD$

.if exists(/usr/local/share/illumos-os-tests)

PROGS =		oclo
PROGS +=	oclo_errors
PROGS +=	ocloexec_verify

LDADD_ocloexec_verify = -lkvm

.PATH: /usr/local/share/illumos-os-tests/tests/oclo
.endif

.include <bsd.regress.mk>
