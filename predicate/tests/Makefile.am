include $(top_srcdir)/build/tests.mk
AUTOMAKE_OPTIONS = subdir-objects

FUNDAMENTALS = ../eval.lo ../dag.lo ../value.lo ../parse.lo ../call_factory.lo

LDADD = \
	$(FUNDAMENTALS) \
    $(builddir)/../../ironbeepp/libibpp.la \
	$(top_builddir)/util/libibutil.la \
	$(top_builddir)/engine/libironbee.la \
    -lboost_system$(BOOST_SUFFIX)
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
    test_standard_list \
    test_standard_math \
    test_standard_predicate \
    test_standard_string \
    test_standard_template \
    test_transform_graph \
    test_validate_graph \
    test_value

EXTRA_TEST = parse_fixture.hpp standard_test.hpp standard_test.cpp

EXTRA_DIST = \
	$(EXTRA_TEST) \
	tc_lua_predicate.rb \
	tc_predicate.rb \
	tc_predicate_constant.rb \
	tc_predicate_errors.rb \
	ts_all.rb \
	ts_lua.rb

TESTS = $(check_PROGRAMS)

# Fundamentals are the portions of Predicate that everything, including
# every fundamental, depends on.

test_eval_SOURCES = test_eval.cpp
test_dag_SOURCES = test_dag.cpp
test_parse_SOURCES = test_parse.cpp
test_call_factory_SOURCES = test_call_factory.cpp
test_value_SOURCES = test_value.cpp

# Non-fundamnetal portions of Predicate

test_bfs_SOURCES = test_bfs.cpp
test_bfs_LDADD = $(LDADD) ../bfs.lo

test_less_SOURCES = test_less.cpp
test_leaves_SOURCES = test_leaves.cpp

test_merge_graph_SOURCES = test_merge_graph.cpp
test_merge_graph_LDADD = $(LDADD) ../libpredicate.la
test_leaves_LDADD = $(LDADD) ../libpredicate.la
test_transform_graph_SOURCES = test_transform_graph.cpp
test_transform_graph_LDADD = $(LDADD) ../libpredicate.la ../standard_boolean.lo
test_validate_graph_SOURCES = test_validate_graph.cpp
test_validate_graph_LDADD = $(LDADD) ../libpredicate.la ../standard_boolean.lo

# Standard is the standard predicate library of functions.
test_standard_development_SOURCES = test_standard_development.cpp
test_standard_development_LDADD = $(LDADD) standard_test.lo ../standard_development.lo ../libpredicate.la
test_standard_boolean_SOURCES = test_standard_boolean.cpp
test_standard_boolean_LDADD = $(LDADD) standard_test.lo ../standard_boolean.lo ../libpredicate.la
test_standard_list_SOURCES = test_standard_list.cpp
test_standard_list_LDADD = $(LDADD) standard_test.lo ../standard_list.lo ../standard_development.lo ../libpredicate.la
test_standard_math_SOURCES = test_standard_math.cpp
test_standard_math_LDADD = $(LDADD) standard_test.lo ../standard_math.lo ../libpredicate.la
test_standard_string_SOURCES = test_standard_string.cpp
test_standard_string_LDADD = $(LDADD) standard_test.lo ../standard_string.lo ../libpredicate.la
test_standard_filter_SOURCES = test_standard_filter.cpp
test_standard_filter_LDADD = $(LDADD) standard_test.lo ../standard_filter.lo ../libpredicate.la
test_standard_predicate_SOURCES = test_standard_predicate.cpp
test_standard_predicate_LDADD = $(LDADD) standard_test.lo ../standard_predicate.lo ../standard_development.lo ../libpredicate.la ../standard_boolean.lo
test_standard_ironbee_SOURCES = test_standard_ironbee.cpp
test_standard_ironbee_LDADD = $(LDADD) standard_test.lo ../standard_ironbee.lo ../standard_predicate.lo ../standard_development.lo ../libpredicate.la
test_standard_template_SOURCES = test_standard_template.cpp
test_standard_template_LDADD = $(LDADD) standard_test.lo ../standard_template.lo ../standard_list.lo ../libpredicate.la

CLEANFILES += lua_test_*.lua

check-local: check-ruby
