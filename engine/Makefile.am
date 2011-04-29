ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
EXTRA_DIST =  config-parser.rl
lib_LTLIBRARIES = libironbee.la
libironbee_la_SOURCES = engine.c provider.c logger.c parser.c data.c tfn.c \
                        config.c config-parser.c config-parser.h core.c \
						matcher.c filter.c \
						config-parser.h ironbee_private.h \
						$(builddir)/lua/ironbee.h
libironbee_la_LIBADD = $(top_builddir)/util/libibutil.la

# Ragel generated files
config-parser.c: config-parser.rl
	if test -n "$(RAGEL)"; then \
	  $(RAGEL) -s -o $@ $<; \
	else \
	  echo "WARNING: $< was modified, but there is no ragel compiler installed to regenerate the $@ file."; \
	fi;
