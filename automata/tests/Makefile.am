include $(top_srcdir)/build/tests.mk

AM_LDFLAGS += \
	-L$(builddir)/.. \
    -lironautomata -liaeudoxus -lprotobuf

# Ignore protobuf warnings.
CPPFLAGS += -Wno-shadow -Wno-extra

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

clean-local:
	rm -rf automata_test_*.*

check-local: ruby-tests
