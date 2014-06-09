ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/servers.mk

EXTRA_DIST = ts_ib.h

if BUILD_TS_SERVER
plugin_LTLIBRARIES = ts_ironbee.la

ts_ironbee_la_SOURCES = \
	ts_plugin.c	\
	ts_module.c	\
	ts_event.c	\
	ts_headers.c	\
	ts_filter.c
ts_ironbee_la_CPPFLAGS = $(AM_CPPFLAGS) @TS_CPPFLAGS@ -I. -I../../servers/trafficserver
ts_ironbee_la_LDFLAGS = -module -avoid-version


install-exec-hook: $(plugin_LTLIBRARIES)
	@echo "Removing unused static libraries..."; \
	for m in $(plugin_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(plugindir)/$$base.*a; \
	  echo TODO: install; \
	done

uninstall-local: $(plugin_LTLIBRARIES)
	@echo "Uninstalling Traffic Server Modules..."; \
	for m in $(plugin_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(plugindir)/$$base.*so; \
	done
endif
