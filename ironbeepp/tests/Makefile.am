ACLOCAL_AMFLAGS = -I../acinclude

AM_CPPFLAGS = $(BOOST_CPPFLAGS) -I$(top_builddir)/include
LDADD = \
    $(top_builddir)/tests/libgtest.a \
    $(top_builddir)/tests/test_main.o \
    $(builddir)/../libibpp.la \
    $(top_builddir)/util/libibutil.la \
    $(top_builddir)/engine/libironbee.la
AM_LDFLAGS = \
    $(BOOST_LDFLAGS) \
    -lstdc++

include $(top_srcdir)/build/tests.mk

check_PROGRAMS = \
    test_catch \
    test_c_trampoline \
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
    test_transaction \
    test_transaction_data \
    test_parsed_request_line \
    test_parsed_response_line \
    test_parsed_name_value \
    test_hooks \
    test_ironbee \
    test_server \
    test_engine \
		test_operator \
		test_hash

TESTS=$(check_PROGRAMS)
CLEANFILES=*.xml *.log

test_catch_SOURCES                = test_catch.cpp
test_c_trampoline_SOURCES         = test_c_trampoline.cpp
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
test_connection_SOURCES           = test_connection.cpp fixture.cpp
test_transaction_SOURCES          = test_transaction.cpp fixture.cpp
test_transaction_data_SOURCES     = test_transaction_data.cpp
test_parsed_request_line_SOURCES  = test_parsed_request_line.cpp fixture.cpp
test_parsed_response_line_SOURCES = test_parsed_response_line.cpp fixture.cpp
test_parsed_name_value_SOURCES    = test_parsed_name_value.cpp
test_hooks_SOURCES                = test_hooks.cpp fixture.cpp
test_ironbee_SOURCES              = test_ironbee.cpp
test_server_SOURCES               = test_server.cpp
test_engine_SOURCES               = test_engine.cpp fixture.cpp
test_operator_SOURCES						  = test_operator.cpp fixture.cpp
test_hash_SOURCES   						  = test_hash.cpp
