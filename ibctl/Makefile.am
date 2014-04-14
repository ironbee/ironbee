include $(top_srcdir)/build/common.mk
bin_PROGRAMS=ibctl

SUBDIRS = tests

ibctl_SOURCES=ibctl.cpp

EXTRA_DIST=tests

ibctl_CPPFLAGS=$(AM_CPPFLAGS)
ibctl_LDFLAGS=$(LDFLAGS) \
    $(top_builddir)/ironbeepp/libibpp.la \
    $(top_builddir)/engine/libironbee.la \
    $(top_builddir)/util/libibutil.la \
    -lboost_program_options$(BOOST_SUFFIX)
