ACLOCAL_AMFLAGS = -I acinclude
SUBDIRS = @TOPLEVEL_SUBDIRS@
if CPP_CODE
SUBDIRS += ironbeepp
endif

include $(top_srcdir)/build/common.mk
EXTRA_DIST = include ironbee_config_auto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ironbee.pc
pkginclude_HEADERS = $(top_srcdir)/include/ironbee/*.h 
doxygen doxygen-pdf:
	@(cd docs && $(MAKE) $@)

manual:
	@(cd docs && $(MAKE) manual)

.PHONY: doxygen doxygen-pdf manual
