ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

pkglibdir = $(libdir)

if FREEBSD
AM_LDFLAGS = -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
AM_LDFLAGS = -module -avoid-version
endif

pkglib_LTLIBRARIES = ibmod_htp.la \
                     ibmod_pcre.la \
                     ibmod_ac.la \
                     ibmod_radix.la \
                     ibmod_binradix.la \
                     ibmod_rules.la \
                     ibmod_user_agent.la

if ENABLE_LUA
pkglib_LTLIBRARIES += ibmod_lua.la
endif

if BUILD_GEOIP
pkglib_LTLIBRARIES += ibmod_geoip.la 
ibmod_geoip_la_SOURCES = geoip.c
ibmod_geoip_la_LIBADD = -lGeoIP -lm
ibmod_geoip_la_LDFLAGS = $(AM_LDFLAGS) $(GEOIP_LDFLAGS)
ibmod_geoip_la_CFLAGS = $(AM_CFLAGS) $(GEOIP_CFLAGS)
ibmod_geoip_la_CPPFLAGS = $(AM_CFLAGS) $(GEOIP_CPPFLAGS)
endif

ibmod_htp_la_SOURCES = modhtp.c
ibmod_htp_la_LIBADD = -lhtp
ibmod_htp_la_LDFLAGS = $(AM_LDFLAGS) $(HTP_LDFLAGS)
ibmod_htp_la_CFLAGS = $(AM_CFLAGS) $(HTP_CFLAGS)

ibmod_pcre_la_SOURCES = pcre.c
ibmod_pcre_la_CPPFLAGS = @PCRE_CPPFLAGS@
ibmod_pcre_la_CFLAGS = @PCRE_CFLAGS@
ibmod_pcre_la_LDFLAGS = $(AM_LDFLAGS) \
                        @PCRE_LDFLAGS@
ibmod_pcre_la_LIBADD = @PCRE_LDADD@

ibmod_ac_la_SOURCES = ac.c
ibmod_ac_la_CFLAGS = ${AM_CFLAGS}
ibmod_ac_la_LDFLAGS = $(AM_LDFLAGS)

ibmod_radix_la_SOURCES = radix.c
ibmod_radix_la_CFLAGS = ${AM_CFLAGS}
ibmod_radix_la_LDFLAGS = $(AM_LDFLAGS)

ibmod_binradix_la_SOURCES = binradix.c
ibmod_binradix_la_CFLAGS = ${AM_CFLAGS}
ibmod_binradix_la_LDFLAGS = $(AM_LDFLAGS)

ibmod_lua_la_SOURCES = lua.c 
ibmod_lua_la_CPPFLAGS = $(CPPFLAGS) \
                        -I$(top_srcdir)/libs/luajit-2.0-ironbee/src -I$(top_srcdir)
ibmod_lua_la_LIBADD = $(top_builddir)/lua/libironbee-lua.la \
                      -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                      -lluajit-ironbee \
                      -lm

ibmod_rules_la_SOURCES = rules.c 
ibmod_rules_la_CPPFLAGS = $(CPPFLAGS) \
                        -I$(top_srcdir) \
                        -D_XOPEN_SOURCE=1

ibmod_rules_la_LIBADD = -lm

if ENABLE_LUA
ibmod_rules_la_SOURCES += \
    rules_lua.c \
    rules_lua_private.h

ibmod_rules_la_CPPFLAGS += \
    $(top_builddir)/lua/libironbee-lua.la \
    -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
    -I$(top_srcdir)/libs/luajit-2.0-ironbee/src
ibmod_rules_la_LIBADD += -lluajit-ironbee \
                         $(top_builddir)/lua/libironbee-lua.la
endif

if BUILD_DEV_MODULES
pkglib_LTLIBRARIES += ibmod_fields.la 
ibmod_fields_la_SOURCES = fields.c
ibmod_fields_la_CFLAGS = ${AM_CFLAGS}
ibmod_fields_la_LDFLAGS = $(AM_LDFLAGS)
endif

if BUILD_DEV_MODULES
pkglib_LTLIBRARIES += ibmod_rule_dev.la 
ibmod_rule_dev_la_SOURCES = rule_dev.c
ibmod_rule_dev_la_CFLAGS = ${AM_CFLAGS}
ibmod_rule_dev_la_LDFLAGS = $(AM_LDFLAGS)
endif

if BUILD_DEV_MODULES
pkglib_LTLIBRARIES += ibmod_perf_stats.la 
ibmod_perf_stats_la_SOURCES = perf_stats.c
ibmod_perf_stats_la_CFLAGS = ${AM_CFLAGS}
ibmod_perf_stats_la_LDFLAGS = $(AM_LDFLAGS)
endif

ibmod_user_agent_la_SOURCES = user_agent.c \
                              user_agent_rules.c \
                              user_agent_private.h 
ibmod_user_agent_la_CFLAGS = ${AM_CFLAGS}
ibmod_user_agent_la_LDFLAGS = $(AM_LDFLAGS)
if FREEBSD
ibmod_user_agent_la_LIBADD = -liconv
endif

install-exec-hook: $(pkglib_LTLIBRARIES)
	@echo "Removing unused static libraries..."; \
	for m in $(pkglib_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(pkglibdir)/$$base.*a; \
	done

uninstall-local: $(pkglib_LTLIBRARIES)
	@echo "Uninstalling Modules..."; \
	for m in $(pkglib_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(pkglibdir)/$$base.*so; \
	done

all-local: $(abs_top_srcdir)/lua/ironbee-ffi.lua $(abs_top_srcdir)/lua/ironbee-api.lua
	mkdir -p $(abs_top_builddir)/modules/.libs
	for f in $(+); do \
		cp "$$f" $(abs_top_builddir)/modules/.libs/`basename "$$f"` ; \
	done

