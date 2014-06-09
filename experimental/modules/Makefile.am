ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

if FREEBSD
AM_LDFLAGS += -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
AM_LDFLAGS += -module -avoid-version
endif

module_LTLIBRARIES = \
	ibmod_rxfilter.la

ibmod_rxfilter_la_SOURCES = rxfilter.c
ibmod_rxfilter_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE
ibmod_rxfilter_la_CPPFLAGS = $(AM_CPPFLAGS)
ibmod_rxfilter_la_LDFLAGS = $(AM_LDFLAGS)

uninstall-local: $(module_LTLIBRARIES)
	@echo "Uninstalling Modules..."; \
	for m in $(module_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(moduledir)/$$base.*so; \
	done
