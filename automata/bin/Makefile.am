ACLOCAL_AMFLAGS = -I ../../acinclude

bin_PROGRAMS = \
    ac_generator \
    bench \
    ec \
    to_dot \
    optimize
      
LDADD = ../libironautomata.la ../libiaeudoxus.la

AM_CPPFLAGS += \
    -I$(srcdir)/../include \
    -I$(builddir)/../include \
	$(PROTOBUF_CPPFLAGS) \
	$(BOOST_CPPFLAGS)
AM_LDFLAGS = \
    $(PROTOBUF_LDFLAGS) \
	$(BOOST_LDFLAGS) \
    -lprotobuf \
    -lboost_program_options \
    -lboost_system \
    -lboost_filesystem \
    -lboost_chrono

ac_generator_SOURCES = ac_generator.cpp
bench_SOURCES = bench.cpp
ec_SOURCES = ec.cpp
to_dot_SOURCES = to_dot.cpp
optimize_SOURCES = optimize.cpp

