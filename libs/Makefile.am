ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

SUBDIRS = @LIBS_SUBDIRS@

BUILT_SOURCES = .build-luajit

clean-local:
if FREEBSD
	(rm -f .build-luajit && cd luajit-2.0-ironbee && gmake distclean)
else
	(rm -f .build-luajit && cd luajit-2.0-ironbee && $(MAKE) distclean)
endif

.build-luajit: luajit-2.0-ironbee/* luajit-2.0-ironbee/src/*
if FREEBSD
	(cd luajit-2.0-ironbee && gmake amalg CCDEBUG=-g CC=$(CC) LDFLAGS="-lssp_nonshared" STATIC_CC=$(CC) DYNAMIC_CC=$(CC) && touch ../.build-luajit)
else
	(cd luajit-2.0-ironbee && $(MAKE) amalg CCDEBUG=-g XCFLAGS=-DLUAJIT_USE_GDBJIT && touch ../.build-luajit)
endif
