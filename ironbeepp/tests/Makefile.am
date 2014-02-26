
include $(top_srcdir)/build/tests.mk

LDADD = \
    $(abs_top_builddir)/ironbeepp/libibpp.la \
    $(abs_top_builddir)/util/libibutil.la \
    $(abs_top_builddir)/engine/libironbee.la
AM_LDFLAGS += -lstdc++

check_PROGRAMS = \
	test_byte_string \
	test_catch \
	test_clock \
	test_configuration_directives \
	test_configuration_map \
	test_configuration_parser \
	test_connection \
	test_c_trampoline \
	test_data \
	test_engine \
	test_field \
	test_hash \
	test_hooks \
	test_ironbee \
	test_list \
	test_memory_pool \
	test_memory_pool_lite \
	test_module \
	test_module_bootstrap_a \
	test_module_bootstrap_b \
	test_module_delegate \
	test_operator \
	test_parsed_header \
	test_parsed_request_line \
	test_parsed_response_line \
  test_parser_suite_adaptors \
	test_server \
	test_site \
	test_throw \
	test_transaction \
	test_transformation \
	test_var \
	test_memory_manager

TESTS = $(check_PROGRAMS)

test_byte_string_SOURCES              = test_byte_string.cpp
test_catch_SOURCES                    = test_catch.cpp
test_clock_SOURCES                    = test_clock.cpp
test_configuration_directives_SOURCES = test_configuration_directives.cpp
test_configuration_map_SOURCES        = test_configuration_map.cpp
test_configuration_parser_SOURCES     = test_configuration_parser.cpp
test_connection_SOURCES               = test_connection.cpp
test_c_trampoline_SOURCES             = test_c_trampoline.cpp
test_data_SOURCES                     = test_data.cpp
test_engine_SOURCES                   = test_engine.cpp
test_field_SOURCES                    = test_field.cpp
test_hash_SOURCES                     = test_hash.cpp
test_hooks_SOURCES                    = test_hooks.cpp
test_ironbee_SOURCES                  = test_ironbee.cpp
test_list_SOURCES                     = test_list.cpp
test_memory_pool_SOURCES              = test_memory_pool.cpp
test_memory_pool_lite_SOURCES         = test_memory_pool_lite.cpp
test_module_bootstrap_a_SOURCES       = test_module_bootstrap_a.cpp
test_module_bootstrap_b_SOURCES       = test_module_bootstrap_b.cpp
test_module_delegate_SOURCES          = test_module_delegate.cpp
test_module_SOURCES                   = test_module.cpp
test_operator_SOURCES                 = test_operator.cpp
test_parsed_header_SOURCES            = test_parsed_header.cpp
test_parsed_request_line_SOURCES      = test_parsed_request_line.cpp
test_parsed_response_line_SOURCES     = test_parsed_response_line.cpp
test_parser_suite_adaptors_SOURCES    = test_parser_suite_adaptors.cpp
test_server_SOURCES                   = test_server.cpp
test_site_SOURCES                     = test_site.cpp
test_throw_SOURCES                    = test_throw.cpp
test_transaction_SOURCES              = test_transaction.cpp
test_transformation_SOURCES           = test_transformation.cpp
test_var_SOURCES                      = test_var.cpp
test_memory_manager_SOURCES           = test_memory_manager.cpp
