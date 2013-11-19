include $(top_srcdir)/build/tests.mk

LDADD = \
    $(builddir)/../libpredicate.la \
    $(builddir)/../../ironbeepp/libibpp.la \
	$(top_builddir)/util/libibutil.la \
	$(top_builddir)/engine/libironbee.la
AM_LDFLAGS += -lstdc++

check_PROGRAMS = \
	test_bfs \
	test_call_factory \
	test_dag \
	test_eval \
	test_leaves \
	test_less \
	test_merge_graph \
	test_parse \
	test_standard_boolean \
	test_standard_development \
	test_standard_filter \
	test_standard_ironbee \
	test_standard_predicate \
	test_standard_template \
	test_standard_valuelist \
	test_transform_graph \
	test_validate_graph

EXTRA_TEST = parse_fixture.hpp standard_test.hpp

TESTS = $(check_PROGRAMS)

test_dag_SOURCES = test_dag.cpp
test_eval_SOURCES = test_eval.cpp
test_standard_boolean_SOURCES = test_standard_boolean.cpp standard_test.cpp
test_standard_development_SOURCES = test_standard_development.cpp standard_test.cpp
test_standard_filter_SOURCES = test_standard_filter.cpp standard_test.cpp
test_standard_ironbee_SOURCES = test_standard_ironbee.cpp standard_test.cpp
test_standard_predicate_SOURCES = test_standard_predicate.cpp standard_test.cpp
test_standard_template_SOURCES = test_standard_template.cpp standard_test.cpp
test_standard_valuelist_SOURCES = test_standard_valuelist.cpp standard_test.cpp
test_call_factory_SOURCES = test_call_factory.cpp
test_parse_SOURCES = test_parse.cpp
test_bfs_SOURCES = test_bfs.cpp
test_merge_graph_SOURCES = test_merge_graph.cpp
test_less_SOURCES = test_less.cpp
test_leaves_SOURCES = test_leaves.cpp
test_transform_graph_SOURCES = test_transform_graph.cpp
test_validate_graph_SOURCES = test_validate_graph.cpp

CLEANFILES += lua_test_*.lua

check-local: check-ruby
