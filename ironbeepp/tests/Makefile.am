ACLOCAL_AMFLAGS = -I../acinclude

EXTRA_DIST = gtest_executor.sh

LDADD = \
    $(top_builddir)/tests/gtest/libgtest.la \
    $(top_builddir)/tests/test_main.o \
    $(builddir)/../libibpp.la \
    $(top_builddir)/util/libibutil.la \
    $(top_builddir)/engine/libironbee.la
    
LDFLAGS = \
    -lstdc++

include $(top_srcdir)/build/tests.mk

TESTS_ENVIRONMENT = ./gtest_executor.sh

BUILT_SOURCES= \
    $(abs_builddir)/gtest_executor.sh

$(abs_builddir)/%: $(srcdir)/%
	if [ "$(builddir)" != "" -a "$(builddir)" != "$(srcdir)" ]; then \
	cp -f $< $@; \
    fi

check_PROGRAMS = \
    test_catch \
    test_data \
    test_module \
    test_module_bootstrap_a \
    test_module_bootstrap_b \
    test_module_delegate \
    test_throw \
    test_memory_pool \
    test_byte_string \
    test_field \
    test_configuration_map \
    test_list \
    test_site \
    test_configuration_parser \
    test_configuration_directives \
    test_clock \
    test_connection \
    test_connection_data \
    test_transaction \
    test_transaction_data \
    test_parsed_request_line \
    test_parsed_response_line \
    test_parsed_name_value
    
TESTS=$(check_PROGRAMS)

test_catch_SOURCES                = test_catch.cpp
test_data_SOURCES                 = test_data.cpp
test_module_SOURCES               = test_module.cpp fixture.cpp
test_module_bootstrap_a_SOURCES   = test_module_bootstrap_a.cpp fixture.cpp
test_module_bootstrap_b_SOURCES   = test_module_bootstrap_b.cpp fixture.cpp
test_module_delegate_SOURCES      = test_module_delegate.cpp fixture.cpp
test_throw_SOURCES                = test_throw.cpp
test_memory_pool_SOURCES          = test_memory_pool.cpp fixture.cpp
test_byte_string_SOURCES          = test_byte_string.cpp
test_field_SOURCES                = test_field.cpp
test_configuration_map_SOURCES    = test_configuration_map.cpp fixture.cpp
test_list_SOURCES                 = test_list.cpp fixture.cpp
test_site_SOURCES                 = test_site.cpp fixture.cpp
test_configuration_parser_SOURCES = \
    test_configuration_parser.cpp fixture.cpp
test_configuration_directives_SOURCES = \
    test_configuration_directives.cpp fixture.cpp
test_clock_SOURCES                = test_clock.cpp
test_connection_SOURCES           = test_connection.cpp
test_connection_data_SOURCES      = test_connection_data.cpp
test_transaction_SOURCES          = test_transaction.cpp
test_transaction_data_SOURCES     = test_transaction_data.cpp
test_parsed_request_line_SOURCES  = test_parsed_request_line.cpp
test_parsed_response_line_SOURCES = test_parsed_response_line.cpp
test_parsed_name_value_SOURCES   = test_parsed_name_value.cpp
