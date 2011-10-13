ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/plugins.mk

pkglibdir = $(prefix)/lib
if BUILD_ATS_PLUGIN
pkglib_LTLIBRARIES = libloader.la ironbee.la

libloader_la_SOURCES = libloader.c
libloader_la_CPPFLAGS = @ATS_CPPFLAGS@
libloader_la_LDFLAGS = -module -avoid-version
libloader_la_CFLAGS = @ATS_CFLAGS@

ironbee_la_SOURCES = ironbee.c
ironbee_la_CPPFLAGS = @ATS_CPPFLAGS@ 
ironbee_la_LDFLAGS = -module -avoid-version
ironbee_la_CFLAGS = @ATS_CFLAGS@

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
