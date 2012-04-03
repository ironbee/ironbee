ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/servers.mk

pkglibdir = $(libdir)
if BUILD_TS_SERVER
pkglib_LTLIBRARIES = libloader.la ironbee.la

libloader_la_SOURCES = libloader.c
libloader_la_CPPFLAGS = @TS_CPPFLAGS@
libloader_la_LDFLAGS = -module -avoid-version

ironbee_la_SOURCES = ironbee.c
ironbee_la_CPPFLAGS = @TS_CPPFLAGS@ 
ironbee_la_LDFLAGS = -module -avoid-version

install-exec-hook: $(pkglib_LTLIBRARIES)
	@echo "Removing unused static libraries..."; \
	for m in $(pkglib_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(pkglibdir)/$$base.*a; \
	  echo TODO: install; \
	done

uninstall-local: $(pkglib_LTLIBRARIES)
	@echo "Uninstalling Traffic Server Modules..."; \
	for m in $(pkglib_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(pkglibdir)/$$base.*so; \
	done
endif
