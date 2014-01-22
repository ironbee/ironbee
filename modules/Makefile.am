ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

SUBDIRS = . tests

AM_LIBADD = $(top_builddir)/engine/libironbee.la

module_DATA =

if FREEBSD
AM_LDFLAGS += -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
AM_LDFLAGS += -module -avoid-version
endif

EXTRA_DIST = \
             parser_suite.hpp \
             persistence_framework.h \
             persistence_framework_private.h \
             txlog.h \
             txlog_json.hpp


module_LTLIBRARIES = ibmod_htp.la \
                     ibmod_pcre.la \
                     ibmod_persistence_framework.la \
                     ibmod_init_collection.la \
                     ibmod_rules.la \
                     ibmod_user_agent.la \
                     ibmod_fast.la \
                     ibmod_libinjection.la \
                     ibmod_log_pipe.la \
                     ibmod_ident.la \
                     ibmod_ident_authbasic.la \
                     ibmod_sqltfn.la
noinst_LTLIBRARIES =

if CPP
noinst_LTLIBRARIES += libparser_suite.la
libparser_suite_la_SOURCES = parser_suite.cpp
module_LTLIBRARIES += ibmod_ps.la
ibmod_ps_la_SOURCES = modps.cpp
ibmod_ps_la_LIBADD = $(AM_LIBADD) libparser_suite.la
endif

if BUILD_GEOIP
module_LTLIBRARIES += ibmod_geoip.la
ibmod_geoip_la_SOURCES = geoip.c
ibmod_geoip_la_LIBADD = $(AM_LIBADD) -lGeoIP $(LIBM_LDFLAGS)
ibmod_geoip_la_LDFLAGS = $(AM_LDFLAGS) $(GEOIP_LDFLAGS)
ibmod_geoip_la_CFLAGS = $(AM_CFLAGS) $(GEOIP_CFLAGS)
ibmod_geoip_la_CPPFLAGS = $(AM_CPPFLAGS) $(GEOIP_CPPFLAGS)
endif

ibmod_persistence_framework_la_SOURCES = persistence_framework.c \
                                         persistence_framework_api.c

ibmod_init_collection_la_SOURCES = init_collection.c \
                                   persistence_framework_api.c
ibmod_init_collection_la_LDFLAGS = $(AM_LDFLAGS)
ibmod_init_collection_la_LIBADD  = $(AM_LIBADD)
ibmod_init_collection_la_CFLAGS  = $(AM_CFLAGS)

ibmod_log_pipe_la_SOURCES = log_pipe.c
ibmod_log_pipe_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE
ibmod_log_pipe_la_CPPFLAGS = $(AM_CPPFLAGS)
ibmod_log_pipe_la_LDFLAGS = $(AM_LDFLAGS)

ibmod_ident_la_SOURCES = ident.c
ibmod_ident_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE
ibmod_ident_la_CPPFLAGS = $(AM_CPPFLAGS)
ibmod_ident_la_LDFLAGS = $(AM_LDFLAGS)

ibmod_ident_authbasic_la_SOURCES = ident_authbasic.c
ibmod_ident_authbasic_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE
ibmod_ident_authbasic_la_CPPFLAGS = $(AM_CPPFLAGS)
ibmod_ident_authbasic_la_LDFLAGS = $(AM_LDFLAGS)

ibmod_htp_la_SOURCES = modhtp.c
ibmod_htp_la_LDFLAGS = $(AM_LDFLAGS)
ibmod_htp_la_LIBADD = $(AM_LIBADD) \
                      $(top_builddir)/libs/libhtp/htp/libhtp.la
ibmod_htp_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -I$(top_builddir)/libs/libhtp/htp \
                        -I$(top_srcdir)/libs/libhtp/htp
ibmod_htp_la_CFLAGS = $(AM_CFLAGS)

ibmod_pcre_la_SOURCES = pcre.c
ibmod_pcre_la_CPPFLAGS = $(AM_CPPFLAGS) @PCRE_CPPFLAGS@
ibmod_pcre_la_CFLAGS = @PCRE_CFLAGS@
ibmod_pcre_la_LDFLAGS = $(AM_LDFLAGS) \
                        @PCRE_LDFLAGS@
ibmod_pcre_la_LIBADD = $(AM_LIBADD) @PCRE_LDADD@

module_LTLIBRARIES += ibmod_ee.la
ibmod_ee_la_SOURCES = ee_oper.c
ibmod_ee_la_LIBADD = $(AM_LIBADD) $(top_builddir)/automata/libiaeudoxus.la
ibmod_ee_la_CFLAGS = ${AM_CFLAGS} -I$(top_srcdir)/automata/include
ibmod_ee_la_LDFLAGS = $(AM_LDFLAGS)

if ENABLE_LUA
module_LTLIBRARIES += ibmod_lua.la
ibmod_lua_la_SOURCES = lua.c \
                       lua_common.c \
                       lua_modules.c \
                       lua_runtime.c \
                       lua_rules.c \
                       lua_private.h \
                       lua_modules_private.h \
                       lua_runtime_private.h \
                       lua_rules_private.h \
                       lua_common_private.h
ibmod_lua_la_CPPFLAGS = -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
                        $(AM_CPPFLAGS)
ibmod_lua_la_LIBADD = $(AM_LIBADD) \
                      -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                      -lluajit-ironbee \
                      $(LIBM_LDFLAGS)

lua_modules  = ibmod_txvars.lua
EXTRA_DIST  += $(lua_modules)
module_DATA += $(lua_modules)
endif

ibmod_rules_la_SOURCES = rules.c
if ENABLE_LUA
ibmod_rules_la_CPPFLAGS = \
    -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
     $(AM_CPPFLAGS) \
     -D_XOPEN_SOURCE=1
else
ibmod_rules_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -D_XOPEN_SOURCE=1
endif

ibmod_rules_la_LIBADD = $(AM_LIBADD) $(LIBM_LDFLAGS)

if ENABLE_LUA
ibmod_rules_la_SOURCES += \
    lua_common.c \
    lua_common_private.h
ibmod_rules_la_LIBADD += -lluajit-ironbee \
                         -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src
endif

if ENABLE_JSON
module_LTLIBRARIES += ibmod_persist.la
ibmod_persist_la_SOURCES = persist.c \
                           persistence_framework_api.c
endif

# Development-only modules defined here
if BUILD_DEV_MODULES

module_LTLIBRARIES       += ibmod_abort.la
ibmod_abort_la_SOURCES    = abort.c
ibmod_abort_la_CFLAGS     = ${AM_CFLAGS}
ibmod_abort_la_LDFLAGS    = $(AM_LDFLAGS)

module_LTLIBRARIES       += ibmod_testops.la
ibmod_testops_la_SOURCES  = testops.c
ibmod_testops_la_CFLAGS   = ${AM_CFLAGS}
ibmod_testops_la_LDFLAGS  = $(AM_LDFLAGS)

endif
# End of development modules

ibmod_user_agent_la_SOURCES = user_agent.c \
                              user_agent_rules.c \
                              user_agent_private.h
ibmod_user_agent_la_CFLAGS = ${AM_CFLAGS}
ibmod_user_agent_la_LDFLAGS = $(AM_LDFLAGS)
if FREEBSD
ibmod_user_agent_la_LIBADD = $(AM_LIBADD) -liconv
endif

ibmod_fast_la_SOURCES = fast.c
ibmod_fast_la_CPPFLAGS = ${AM_CPPFLAGS} -I$(srcdir)/../automata/include
ibmod_fast_la_LIBADD = $(AMLIB_ADD) ../automata/libiaeudoxus.la

libinjection_sqli.c: $(abs_top_srcdir)/libs/libinjection/c/libinjection_sqli.c
	cp $< $@

ibmod_libinjection_la_SOURCES = libinjection.c libinjection_sqli.c
ibmod_libinjection_la_CFLAGS = ${AM_CFLAGS} -I$(abs_top_srcdir)/libs/libinjection/c
ibmod_libinjection_la_LDFLAGS = $(AM_LDFLAGS)

ibmod_sqltfn_la_SOURCES = sqltfn.c
ibmod_sqltfn_la_CFLAGS = ${AM_CFLAGS} -I$(abs_top_srcdir)/libs/sqltfn/src
ibmod_sqltfn_la_LDFLAGS = $(AM_LDFLAGS)
ibmod_sqltfn_la_LIBADD = ../libs/sqltfn/src/libsqltfn.la

if CPP
module_LTLIBRARIES += ibmod_engine_shutdown.la
ibmod_engine_shutdown_la_SOURCES = \
    engine_shutdown.cpp
ibmod_engine_shutdown_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
ibmod_engine_shutdown_la_LIBADD = $(AM_LIBADD) \
    $(top_builddir)/ironbeepp/libibpp.la
endif

if CPP
module_LTLIBRARIES += ibmod_xrules.la
ibmod_xrules_la_SOURCES = xrules.cpp
ibmod_xrules_la_CPPFLAGS = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)
ibmod_xrules_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version \
			  $(BOOST_LDFLAGS) \
			  -lboost_regex$(BOOST_SUFFIX) \
			  -lboost_date_time$(BOOST_SUFFIX)
ibmod_xrules_la_LIBADD  = $(AM_LIBADD) \
			  $(top_builddir)/ironbeepp/libibpp.la
endif

if CPP
module_LTLIBRARIES += ibmod_error_page.la
ibmod_error_page_la_SOURCES = error_page.cpp
ibmod_error_page_la_CPPFLAGS = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)
ibmod_error_page_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version \
			      $(BOOST_LDFLAGS) \
			      -lboost_iostreams$(BOOST_SUFFIX) \
ibmod_error_page_la_LIBADD  = $(AM_LIBADD) \
			      $(top_builddir)/ironbeepp/libibpp.la
endif

if CPP
module_LTLIBRARIES += ibmod_txlog.la
ibmod_txlog_la_SOURCES = txlog.cpp txlog_json.cpp
ibmod_txlog_la_CPPFLAGS = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)
ibmod_txlog_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version \
			 $(BOOST_LDFLAGS)
ibmod_txlog_la_LIBADD  = $(AM_LIBADD) \
                         $(top_builddir)/ironbeepp/libibpp.la
endif

if CPP
module_LTLIBRARIES += ibmod_trusted_proxy.la
ibmod_trusted_proxy_la_SOURCES = \
    trusted_proxy.cpp
ibmod_trusted_proxy_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
ibmod_trusted_proxy_la_LIBADD = $(AM_LIBADD) \
    $(top_builddir)/ironbeepp/libibpp.la
endif

install-exec-hook: $(module_LTLIBRARIES)
	@echo "Removing unused static libraries..."; \
	for m in $(module_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(moduledir)/$$base.*a; \
	done

uninstall-local: $(module_LTLIBRARIES)
	@echo "Uninstalling Modules..."; \
	for m in $(module_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(moduledir)/$$base.*so; \
	done

if ENABLE_LUA
all-local: $(abs_top_builddir)/lua/ironbee-ffi-h.lua
	mkdir -p $(abs_top_builddir)/modules/.libs
	for f in $(+); do \
		cp "$$f" $(abs_top_builddir)/modules/.libs/`basename "$$f"` ; \
	done
	cp "$(top_srcdir)"/modules/*.lua "$(top_builddir)"/modules/.libs/
	cp -R $(abs_top_srcdir)/lua/ironbee $(abs_top_builddir)/modules/.libs
endif
