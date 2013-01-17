
ACLOCAL_AMFLAGS= -I m4

SUBDIRS= $(GENERIC_LIBRARY_NAME) test docs

DIST_SUBDIRS = $(GENERIC_LIBRARY_NAME) test docs
EXTRA_DIST = LICENSE NOTICE docs/doxygen.conf docs/QUICK_START 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = htp.pc

test: all
	@(cd test && $(MAKE) $@)

test-compile-only: all
	@(cd test && $(MAKE) $@)

check-compile-only: all
	@(cd test && $(MAKE) $@)

doxygen doxygen-pdf:
	@(cd docs && $(MAKE) $@)
