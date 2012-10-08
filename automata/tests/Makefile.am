include $(top_srcdir)/build/tests.mk

LDFLAGS += $(BOOST_LDFLAGS) -L$(top_builddir)/tests -L$(builddir)/.. \
    -lironautomata -liaeudoxus
CPPFLAGS += $(BOOST_CPPFLAGS) -I$(top_srcdir)/tests \
    -I$(srcdir)/../include -I$(builddir)/../include
LDADD = $(LIBUTIL_LDADD) \
    $(top_builddir)/tests/libgtest.a \
    $(top_builddir)/tests/gtest_main.o

.PHONY: test
test: check

# Ignore protobuf warnings.
CPPFLAGS += -Wno-shadow -Wno-extra

TESTS_ENVIRONMENT = $(top_srcdir)/tests/gtest_executor.sh

check_PROGRAMS = \
    test_bits \
    test_buffer \
    test_intermediate \
    test_optimize_edges \
    test_vls
    
test_bits_SOURCES = test_bits.cpp
test_buffer_SOURCES = test_buffer.cpp
test_intermediate_SOURCES = test_intermediate.cpp
test_optimize_edges_SOURCES = test_optimize_edges.cpp
test_vls_SOURCES = test_vls.cpp

TESTS = $(check_PROGRAMS)

check-local:
	(cd $(srcdir); abs_builddir=$(abs_builddir) $(ruby) ./ts_all.rb --verbose $(test_args))
