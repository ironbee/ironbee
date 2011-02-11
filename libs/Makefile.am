ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

SUBDIRS = @LIBS_SUBDIRS@

BUILT_SOURCES = .build-luajit

clean-local:
	(rm -f .build-luajit && cd luajit-2.0-ironbee && $(MAKE) distclean)

.build-luajit: luajit-2.0-ironbee/* luajit-2.0-ironbee/src/*
	(cd luajit-2.0-ironbee && $(MAKE) amalg && touch ../.build-luajit)
