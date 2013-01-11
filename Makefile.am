ACLOCAL_AMFLAGS = -I acinclude
SUBDIRS = @TOPLEVEL_SUBDIRS@

include $(top_srcdir)/build/common.mk
EXTRA_DIST = include ironbee_config_auto.h

RPM_RELEASE ?= 1

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ironbee.pc
# Sort to eliminate duplicates if top_srcdir == top_builddir
pkginclude_HEADERS = $(sort $(wildcard $(top_srcdir)/include/ironbee/*.h) $(top_builddir)/include/ironbee/release.h)
doxygen doxygen-pdf:
	@(cd docs && $(MAKE) $@)

manual:
	@(cd docs && $(MAKE) manual)

luajit:
	@(cd libs && $(MAKE) )

.PHONY: doxygen doxygen-pdf manual luajit

rpm_topdir=`cd $(top_builddir) && pwd`/packaging/rpm
rpm-package: dist
	if test x$(HAVE_RPM) = xyes ; then \
		cd $(rpm_topdir) ; \
		mkdir -p BUILD RPMS SOURCES ; \
		cd SOURCES ; \
		ln -s ../../../$(distdir).tar.gz ; \
		cd .. ; \
		rpmbuild --define "_topdir `cd . && pwd`" --define "_release $(RPM_RELEASE)" -ba ironbee.spec && \
		echo "Package successfully built in `pwd`/RPMS." ; \
	else \
		echo "Error: RPM executable and/or source directory not found." ; \
	fi

if CPP
ironbeepp_include_HEADERS = \
    $(top_srcdir)/include/ironbeepp/*.hpp
endif
