include $(top_srcdir)/build/tests.mk

AM_LDFLAGS += \
	-L$(builddir)/.. \
    -lironautomata -liaeudoxus -lprotobuf

# Ignore protobuf warnings.
AM_CPPFLAGS += \
	-Wno-shadow -Wno-extra \
	-I$(srcdir)/../include \
	-I$(builddir)/../include

check_PROGRAMS = \
    test_bits \
    test_buffer \
    test_intermediate \
    test_optimize_edges \
    test_vls

EXTRA_DIST = \
	automata_test.rb \
	tc_basic.rb \
	tc_pattern.rb \
	ts_all.rb

test_bits_SOURCES = test_bits.cpp
test_buffer_SOURCES = test_buffer.cpp
test_intermediate_SOURCES = test_intermediate.cpp
test_optimize_edges_SOURCES = test_optimize_edges.cpp
test_vls_SOURCES = test_vls.cpp

TESTS = $(check_PROGRAMS)

clean-local:
	rm -rf automata_test_*.*

check-local: check-ruby
