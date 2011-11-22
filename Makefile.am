
ACLOCAL_AMFLAGS= -I m4

SUBDIRS= $(GENERIC_LIBRARY_NAME) test docs

DIST_SUBDIRS = $(GENERIC_LIBRARY_NAME) test docs

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = htp.pc

doxygen doxygen-pdf:
	@(cd docs && $(MAKE) $@)
