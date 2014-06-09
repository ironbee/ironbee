ACLOCAL_AMFLAGS = -I ../../acinclude

bin_PROGRAMS = \
    ac_generator \
    ee \
    ec \
    to_dot \
    optimize \
    trie_generator

EXTRA_DIST = ac_generator.rb

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
    -lboost_program_options$(BOOST_SUFFIX) \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_filesystem$(BOOST_SUFFIX) \
    -lboost_chrono$(BOOST_SUFFIX)

ac_generator_SOURCES = ac_generator.cpp
ee_SOURCES = ee.cpp
ec_SOURCES = ec.cpp
to_dot_SOURCES = to_dot.cpp
optimize_SOURCES = optimize.cpp
trie_generator_SOURCES = trie_generator.cpp

