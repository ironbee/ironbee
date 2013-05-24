include $(top_srcdir)/build/tests.mk

LDADD = \
    $(builddir)/../libpredicate.la \
    $(builddir)/../../ironbeepp/libibpp.la
AM_LDFLAGS += -lstdc++

check_PROGRAMS = \
	test_bfs \
	test_call_factory \
	test_dag \
	test_leaves \
	test_less \
	test_merge_graph \
	test_parse \
	test_standard \
	test_transform_graph \
	test_validate \
	test_validate_graph

EXTRA_TEST = parse_fixture.hpp

TESTS = $(check_PROGRAMS)

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

check-local: ruby-tests
