ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

SUBDIRS = @LIBS_SUBDIRS@
DIST_SUBDIRS = 
BUILT_SOURCES =

if ENABLE_LUA
BUILT_SOURCES += .build-luajit
endif

clean-local:
if ENABLE_LUA
if FREEBSD
	(rm -f .build-luajit && cd luajit-2.0-ironbee && gmake clean)
else
	(rm -f .build-luajit && cd luajit-2.0-ironbee && $(MAKE) clean)
endif
endif

distclean-local: clean-local
	for sdir in $(SUBDIRS); do \
	    (cd $$sdir && $(MAKE) distclean ); \
	done

.build-luajit: $(srcdir)/luajit-2.0-ironbee/* $(srcdir)/luajit-2.0-ironbee/src/*
	if [ "$(builddir)" != "" -a "$(builddir)" != "$(srcdir)" ]; then \
		cp -rp $(srcdir)/luajit-2.0-ironbee $(builddir); \
		chmod -R u+w $(builddir)/luajit-2.0-ironbee; \
	fi;
if ENABLE_LUA
if FREEBSD
	(cd luajit-2.0-ironbee && gmake amalg TARGET_STRIP=@: CCDEBUG=-g CC=$(CC) LDFLAGS="-lssp_nonshared" STATIC_CC=$(CC) DYNAMIC_CC=$(CC) && touch ../.build-luajit)
else
	(cd luajit-2.0-ironbee && $(MAKE) amalg TARGET_STRIP=@: CCDEBUG=-g XCFLAGS=-DLUAJIT_USE_GDBJIT && touch ../.build-luajit)
endif
endif

EXTRA_DIST = luajit-2.0-ironbee libhtp libinjection sqltfn
