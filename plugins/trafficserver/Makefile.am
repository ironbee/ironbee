ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/plugins.mk

pkglibdir = $(prefix)/lib
pkglib_LTLIBRARIES = libloader.la ironbee.la

libloader_la_SOURCES = libloader.c
libloader_la_CFLAGS = @TS_CFLAGS@

ironbee_la_SOURCES = ironbee.c
ironbee_la_CFLAGS = @TS_CFLAGS@

install-exec-hook: $(pkglib_LTLIBRARIES)
	@echo "Removing unused static libraries..."; \
	for m in $(pkglib_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(pkglibdir)/$$base.*a; \
	  echo TODO: cp -p $(DESTDIR)$(pkglibdir)/$$base.so; \
	done

uninstall-local: $(pkglib_LTLIBRARIES)
	@echo "Uninstalling Apache Module..."; \
	for m in $(pkglib_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(pkglibdir)/$$base.*so; \
	done

