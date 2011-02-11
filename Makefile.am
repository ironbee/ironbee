ACLOCAL_AMFLAGS = -I acinclude
SUBDIRS = @TOPLEVEL_SUBDIRS@

include $(top_srcdir)/build/common.mk

doxygen doxygen-pdf:
	@(cd docs && $(MAKE) $@)

manual:
	@(cd docs && $(MAKE) manual)

.PHONY: doxygen doxygen-pdf manual
