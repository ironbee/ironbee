ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/plugins.mk

pkglibdir = $(prefix)/lib
pkglib_LTLIBRARIES = mod_ironbee.la

mod_ironbee_la_SOURCES = mod_ironbee.c

mod_ironbee_la_CFLAGS = @APXS_CFLAGS@ @APR_CFLAGS@ @APU_CFLAGS@
mod_ironbee_la_CPPFLAGS = @APR_CPPFLAGS@
mod_ironbee_la_LIBADD = @APR_LDADD@ @APU_LDADD@ \
                           $(top_builddir)/engine/libironbee.la
mod_ironbee_la_LDFLAGS = -no-undefined -module -avoid-version \
                            @APR_LDFLAGS@ @APU_LDFLAGS@ @APXS_LDFLAGS@

install-exec-hook: $(pkglib_LTLIBRARIES)
	@echo "Removing unused static libraries..."; \
	for m in $(pkglib_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(pkglibdir)/$$base.*a; \
	  echo TODO: cp -p $(DESTDIR)$(pkglibdir)/$$base.so $(APXS_MODULES); \
	done
