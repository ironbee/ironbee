ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/servers.mk

if BUILD_HTTPD_SERVER
plugin_LTLIBRARIES = mod_ironbee.la mod_range_filter.la

mod_ironbee_la_SOURCES = mod_ironbee.c

mod_ironbee_la_CFLAGS = @APXS_CFLAGS@ @APR_CFLAGS@ @APU_CFLAGS@
mod_ironbee_la_CPPFLAGS = $(AM_CPPFLAGS) @APR_CPPFLAGS@
mod_ironbee_la_LIBADD = @APR_LDADD@ @APU_LDADD@ \
                        $(top_builddir)/engine/libironbee.la
mod_ironbee_la_LDFLAGS = -module -avoid-version \
						 $(LIBM_LDFLAGS) \
                         @APR_LDFLAGS@ @APU_LDFLAGS@ @APXS_LDFLAGS@


mod_range_filter_la_SOURCES = mod_range_filter.c

mod_range_filter_la_CFLAGS = @APXS_CFLAGS@ @APR_CFLAGS@ @APU_CFLAGS@
mod_range_filter_la_CPPFLAGS = $(AM_CPPFLAGS) @APR_CPPFLAGS@
mod_range_filter_la_LIBADD = @APR_LDADD@ @APU_LDADD@ \
                        $(top_builddir)/engine/libironbee.la
mod_range_filter_la_LDFLAGS = -module -avoid-version \
                         $(LIBM_LDFLAGS) \
                         @APR_LDFLAGS@ @APU_LDFLAGS@ @APXS_LDFLAGS@

if HAVE_UNUSED_CONST_VARIABLE
mod_range_filter_la_CFLAGS += -Wno-unused-const-variable
endif

install-exec-hook: $(plugin_LTLIBRARIES)
	@echo "Removing unused static libraries..."; \
	for m in $(plugin_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(plugindir)/$$base.*a; \
	  echo TODO: cp -p $(DESTDIR)$(plugindir)/$$base.so $(APXS_MODULES); \
	done

uninstall-local: $(plugin_LTLIBRARIES)
	@echo "Uninstalling Apache Module..."; \
	for m in $(plugin_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(plugindir)/$$base.*so; \
	done
endif
