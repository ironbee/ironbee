ACLOCAL_AMFLAGS = -I acinclude
SUBDIRS = @TOPLEVEL_SUBDIRS@

include $(top_srcdir)/build/common.mk
EXTRA_DIST = include ironbee_config_auto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ironbee.pc
pkginclude_HEADERS = $(top_srcdir)/include/ironbee/ahocorasick.h \
                     $(top_srcdir)/include/ironbee/build.h \
                     $(top_srcdir)/include/ironbee/cfgmap.h \
                     $(top_srcdir)/include/ironbee/debug.h \
                     $(top_srcdir)/include/ironbee/engine.h \
                     $(top_srcdir)/include/ironbee/hash.h \
                     $(top_srcdir)/include/ironbee/logformat.h \
                     $(top_srcdir)/include/ironbee/module_sym.h \
                     $(top_srcdir)/include/ironbee/plugin.h \
                     $(top_srcdir)/include/ironbee/radix.h  \
                     $(top_srcdir)/include/ironbee/stream.h \
                     $(top_srcdir)/include/ironbee/types.h \
                     $(top_srcdir)/include/ironbee/uuid.h \
                     $(top_srcdir)/include/ironbee/array.h \
                     $(top_srcdir)/include/ironbee/bytestr.h \
                     $(top_srcdir)/include/ironbee/config.h \
                     $(top_srcdir)/include/ironbee/dso.h \
                     $(top_srcdir)/include/ironbee/field.h \
                     $(top_srcdir)/include/ironbee/list.h \
                     $(top_srcdir)/include/ironbee/module.h \
                     $(top_srcdir)/include/ironbee/mpool.h \
                     $(top_srcdir)/include/ironbee/provider.h \
                     $(top_srcdir)/include/ironbee/release.h \
                     $(top_srcdir)/include/ironbee/string.h \
                     $(top_srcdir)/include/ironbee/util.h

doxygen doxygen-pdf:
	@(cd docs && $(MAKE) $@)

manual:
	@(cd docs && $(MAKE) manual)

.PHONY: doxygen doxygen-pdf manual
