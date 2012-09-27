LDFLAGS += $(BOOST_LDFLAGS) -L$(top_builddir)/tests -L$(builddir)/.. \
    -lironautomata
CPPFLAGS += $(BOOST_CPPFLAGS) -I$(top_srcdir)/tests \
    -I$(srcdir)/../include -I$(builddir)/../include
LDADD = $(LIBUTIL_LDADD) \
    $(top_builddir)/tests/libgtest.a \
    $(top_builddir)/tests/gtest_main.o

.PHONY: test
test: check

TESTS_ENVIRONMENT = $(top_srcdir)/tests/gtest_executor.sh

check_PROGRAMS = \
    test_bits \
    test_buffer \
    test_intermediate \
    test_vls
    
test_bits_SOURCES = test_bits.cpp
test_buffer_SOURCES = test_buffer.cpp
test_intermediate_SOURCES = test_intermediate.cpp
test_vls_SOURCES = test_vls.cpp

TESTS = $(check_PROGRAMS)
