include $(top_srcdir)/build/common.mk

pkglibdir = $(libdir)

SUBDIRS = tests

AM_LDFLAGS += \
  	$(BOOST_LDFLAGS) \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_thread-mt

pkglib_LTLIBRARIES = ibmod_predicate.la
noinst_LTLIBRARIES = libpredicate.la
libpredicate_la_SOURCES = \
    dag.cpp \
    call_factory.cpp \
    parse.cpp \
    standard.cpp \
    dot.cpp \
    bfs.cpp \
    merge_graph.cpp \
    reporter.cpp \
    validate.cpp \
    transform_graph.cpp \
		validate_graph.cpp \
		pre_eval_graph.cpp

EXTRA_DIST = \
		bfs.hpp                 \
		call_factory.hpp        \
		dag.hpp                 \
		dot.hpp                 \
		ibmod_predicate.hpp     \
		ironbee.hpp             \
		leaves.hpp              \
		less.hpp                \
		merge_graph.hpp         \
		parse.hpp               \
		pre_eval_graph.hpp      \
		reporter.hpp            \
		standard.hpp            \
		transform_graph.hpp     \
		validate.hpp            \
		validate_graph.hpp

ibmod_predicate_la_SOURCES = ibmod_predicate.cpp
ibmod_predicate_la_LIBADD = libpredicate.la
if FREEBSD
ibmod_predicate_la_LDFLAGS = $(AM_LDFLAGS) -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
ibmod_predicate_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
endif
