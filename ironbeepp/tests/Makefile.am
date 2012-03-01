ACLOCAL_AMFLAGS = -I../acinclude

AM_CPPFLAGS += -I$(srcdir)/..

LDADD = \
    $(top_builddir)/tests/gtest/libgtest.la \
    $(top_builddir)/tests/test_main.o \
    $(builddir)/../libibpp.la \
    $(top_builddir)/util/libibutil.la \
    $(top_builddir)/engine/libironbee.la
    
LDFLAGS = \
    -lstdc++

include $(top_srcdir)/build/tests.mk

check_PROGRAMS = \
    test_catch \
    test_data \
    test_module \
    test_module_bootstrap_a \
    test_module_bootstrap_b \
    test_module_delegate \
    test_throw \
    test_memory_pool \
    test_byte_string
    
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

check-local: $(check_PROGRAMS)
	for cp in $(check_PROGRAMS); do \
		./$${cp} --gtest_output=xml:$${cp}_details.xml 2> $${cp}_stderr.log; \
		if test -n "$(VALGRIND)"; then \
		 $(VALGRIND) --xml=yes --xml-file=$${cp}_valgrind_memcheck.xml ./$${cp}; \
		fi; \
	done
