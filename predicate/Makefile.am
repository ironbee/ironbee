include $(top_srcdir)/build/common.mk

SUBDIRS = tests

AM_LDFLAGS += \
  	$(BOOST_LDFLAGS) \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_thread-mt

module_LTLIBRARIES = ibmod_predicate.la
noinst_LTLIBRARIES = libpredicate.la
libpredicate_la_SOURCES = \
    dag.cpp \
    call_factory.cpp \
    call_helpers.cpp \
    parse.cpp \
    standard.cpp \
    standard_boolean.cpp \
    standard_development.cpp \
    standard_filter.cpp \
    standard_ironbee.cpp \
    standard_valuelist.cpp \
    standard_predicate.cpp \
    dot.cpp \
    bfs.cpp \
    meta_call.cpp \
    merge_graph.cpp \
    reporter.cpp \
    validate.cpp \
    transform_graph.cpp \
	validate_graph.cpp \
	pre_eval_graph.cpp

EXTRA_DIST = \
		bfs.hpp                   \
		call_factory.hpp          \
		call_helpers.hpp          \
		dag.hpp                   \
		dot.hpp                   \
		ibmod_predicate.hpp       \
		ironbee.hpp               \
		leaves.hpp                \
		less.hpp                  \
		meta_call.hpp             \
		merge_graph.hpp           \
		parse.hpp                 \
		pre_eval_graph.hpp        \
		reporter.hpp              \
		standard.hpp              \
		standard_boolean.hpp      \
		standard_development.hpp  \
		standard_filter.hpp       \
		standard_ironbee.hpp      \
		standard_valuelist.hpp    \
		standard_predicate.hpp    \
		transform_graph.hpp       \
		validate.hpp              \
		validate_graph.hpp

ibmod_predicate_la_SOURCES = ibmod_predicate.cpp
ibmod_predicate_la_LIBADD = libpredicate.la
if FREEBSD
ibmod_predicate_la_LDFLAGS = $(AM_LDFLAGS) -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
ibmod_predicate_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
endif
