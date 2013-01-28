
include $(top_srcdir)/build/common.mk

ACLOCAL_AMFLAGS = -I ../../acinclude

if CPP

bin_PROGRAMS = generate

LDADD = ../automata/libironautomata.la ../automata/libiaeudoxus.la

AM_CPPFLAGS += \
    -I$(srcdir)/../automata/include \
    -I$(builddir)/../automata/include \
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

generate_SOURCES = generate.cpp

endif
