ACLOCAL_AMFLAGS = -I../acinclude

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
    test_configuration_map
    
TESTS=$(check_PROGRAMS)

test_catch_SOURCES              = test_catch.cpp
test_data_SOURCES               = test_data.cpp
test_module_SOURCES             = test_module.cpp fixture.cpp
test_module_bootstrap_a_SOURCES = test_module_bootstrap_a.cpp fixture.cpp
test_module_bootstrap_b_SOURCES = test_module_bootstrap_b.cpp fixture.cpp
test_module_delegate_SOURCES    = test_module_delegate.cpp fixture.cpp
test_throw_SOURCES              = test_throw.cpp
test_memory_pool_SOURCES        = test_memory_pool.cpp fixture.cpp
test_byte_string_SOURCES        = test_byte_string.cpp
test_field_SOURCES              = test_field.cpp
test_configuration_map_SOURCES  = test_configuration_map.cpp fixture.cpp

