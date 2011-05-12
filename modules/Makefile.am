ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

pkglibdir = $(prefix)/lib

if FREEBSD
AM_LDFLAGS = -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
AM_LDFLAGS = -module -avoid-version
endif

pkglib_LTLIBRARIES = ibmod_htp.la \
                     ibmod_pcre.la \
                     ibmod_lua.la \
                     ibmod_poc_sig.la

ibmod_htp_la_SOURCES = htp.c
ibmod_htp_la_LIBADD = -lhtp
ibmod_htp_la_LDFLAGS = $(AM_LDFLAGS) $(HTP_LDFLAGS)
ibmod_htp_la_CFLAGS = $(AM_CFLAGS) $(HTP_CFLAGS)

ibmod_pcre_la_SOURCES = pcre.c
ibmod_pcre_la_CPPFLAGS = @PCRE_CPPFLAGS@
ibmod_pcre_la_CFLAGS = @PCRE_CFLAGS@
ibmod_pcre_la_LDFLAGS = $(AM_LDFLAGS) \
                        @PCRE_LDFLAGS@
ibmod_pcre_la_LIBADD = @PCRE_LDADD@

ibmod_lua_la_SOURCES = lua.c 
ibmod_lua_la_CPPFLAGS = $(CPPFLAGS) \
                        -I$(top_srcdir)/libs/luajit-2.0-ironbee/src -I$(top_srcdir)
ibmod_lua_la_LIBADD = $(top_builddir)/lua/libironbee-lua.la \
                      -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                      -lluajit-ironbee \
                      -lm

ibmod_poc_sig_la_SOURCES = poc_sig.c 
ibmod_poc_sig_la_LDFLAGS = $(AM_LDFLAGS)
ibmod_poc_sig_la_CFLAGS = $(AM_CFLAGS)

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

