ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk


EXTRA_DIST =  config-parser.rl
lib_LTLIBRARIES = libironbee.la
libironbee_la_SOURCES = engine.c provider.c logger.c parser.c data.c \
                        config.c config-parser.c config-parser.h core.c \
                        matcher.c filter.c \
                        operator.c action.c transformation.c \
                        module.c \
                        parsed_content.c \
                        core_tfns.c \
                        core_operators.c \
                        core_actions.c \
                        rule_engine.c \
                        state_notify.c \
                        config-parser.h \
                        ironbee_private.h \
                        $(top_builddir)/lua/ironbee.h
libironbee_la_LIBADD = $(top_builddir)/util/libibutil.la

if DARWIN
install-exec-hook: $(lib_LTLIBRARIES)
	@for l in $(lib_LTLIBRARIES); do \
	  base=`echo $$l | sed 's/\..*//'`; \
	  echo "rebasing $$base..."; \
	  rebase -v $(DESTDIR)$(libdir)/$$base.dylib; \
	done
endif


# Ragel generated files
RUNRAGEL=$(srcdir)/run-ragel.py
$(srcdir)/config-parser.c: $(srcdir)/config-parser.rl
	$(RUNRAGEL) "$(RAGEL)" $*

config-parser.dot: $(srcdir)/config-parser.rl
	$(RAGEL) -V $< > $@

config-parser.svg: config-parser.dot
	dot -Tsvg $<  > $@
