ACLOCAL_AMFLAGS = -I../acinclude

AM_CPPFLAGS += $(BOOST_CPPFLAGS) -I$(top_builddir)/include
LDADD = \
    $(top_builddir)/tests/libgtest.a \
    $(top_builddir)/tests/test_main.o \
    $(builddir)/../libpredicate.la \
    $(builddir)/../../ironbeepp/libibpp.la \
    $(top_builddir)/util/libibutil.la \
    $(top_builddir)/engine/libironbee.la
AM_LDFLAGS = \
    $(BOOST_LDFLAGS) \
    -lstdc++

include $(top_srcdir)/build/tests.mk

check_PROGRAMS = \
    test_dag \
    test_call_factory \
    test_parse \
    test_standard \
    test_bfs \
    test_merge_graph \
    test_validate \
    test_less \
    test_leaves \
    test_transform_graph \
		test_validate_graph

TESTS=$(check_PROGRAMS)
CLEANFILES=*.xml *.log

test_dag_SOURCES = test_dag.cpp $(top_srcdir)/ironbeepp/tests/fixture.cpp
test_standard_SOURCES = test_standard.cpp $(top_srcdir)/ironbeepp/tests/fixture.cpp
test_call_factory_SOURCES = test_call_factory.cpp
test_parse_SOURCES = test_parse.cpp
test_bfs_SOURCES = test_bfs.cpp
test_merge_graph_SOURCES = test_merge_graph.cpp
test_validate_SOURCES = test_validate.cpp
test_less_SOURCES = test_less.cpp
test_leaves_SOURCES = test_leaves.cpp
test_transform_graph_SOURCES = test_transform_graph.cpp
test_validate_graph_SOURCES = test_validate_graph.cpp
