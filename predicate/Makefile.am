include $(top_srcdir)/build/common.mk

SUBDIRS = tests

AM_LDFLAGS += \
  $(BOOST_LDFLAGS)              \
  -lboost_system$(BOOST_SUFFIX) \
  -lboost_thread$(BOOST_THREAD_SUFFIX) \
  -lboost_regex$(BOOST_SUFFIX)

module_LTLIBRARIES = ibmod_predicate.la
noinst_LTLIBRARIES = libpredicate.la libpredicate_standard.la
libpredicate_la_SOURCES = \
    dag.cpp                  \
    eval.cpp                 \
    call_factory.cpp         \
    call_helpers.cpp         \
    parse.cpp                \
    dot.cpp                  \
    dot2.cpp                 \
    functional.cpp           \
    bfs.cpp                  \
    meta_call.cpp            \
    merge_graph.cpp          \
    reporter.cpp             \
    validate.cpp             \
    transform_graph.cpp      \
    validate_graph.cpp       \
    pre_eval_graph.cpp       \
    tree_copy.cpp            \
    value.cpp                
libpredicate_standard_la_SOURCES = \
    standard.cpp             \
    standard_boolean.cpp     \
    standard_development.cpp \
    standard_filter.cpp      \
    standard_ironbee.cpp     \
    standard_template.cpp    \
    standard_list.cpp        \
    standard_predicate.cpp   \
    standard_string.cpp      
noinst_PROGRAMS = pp_dot

EXTRA_DIST = \
		bfs.hpp                   \
		call_factory.hpp          \
		call_helpers.hpp          \
		dag.hpp                   \
        eval.hpp                  \
		dot.hpp                   \
		dot2.hpp                  \
        functional.hpp            \
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
		standard_template.hpp     \
		standard_list.hpp         \
		standard_predicate.hpp    \
		standard_string.hpp       \
		tree_copy.hpp             \
		transform_graph.hpp       \
		validate.hpp              \
		validate_graph.hpp        \
        value.hpp

ibmod_predicate_la_SOURCES = ibmod_predicate.cpp
ibmod_predicate_la_LIBADD = libpredicate.la libpredicate_standard.la
if FREEBSD
ibmod_predicate_la_LDFLAGS = $(AM_LDFLAGS) -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
ibmod_predicate_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
endif

pp_dot_SOURCES = pp_dot.cpp
pp_dot_LDADD = \
    libpredicate.la                       \
    $(top_builddir)/ironbeepp/libibpp.la  \
    $(top_builddir)/util/libibutil.la     \
    $(top_builddir)/engine/libironbee.la
pp_dot_LDFLAGS = $(AM_LDFLAGS) -lboost_program_options$(BOOST_SUFFIX)

pp_dot_CPPFLAGS = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)
