SUBDIRS = . tests

ACLOCAL_AMFLAGS = -I ../../acinclude

lib_LTLIBRARIES = libironautomata.la
libironautomata_la_SOURCES = \
	buffer.cpp

AM_CPPFLAGS = \
	$(PROTOBUF_CPPFLAGS) \
	$(BOOST_CPPFLAGS) \
    -I$(srcdir)/include
AM_LDFLAGS = \
	$(BOOST_LDFLAGS)
