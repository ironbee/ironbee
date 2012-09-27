SUBDIRS = . tests

ACLOCAL_AMFLAGS = -I ../../acinclude

lib_LTLIBRARIES = libironautomata.la
libironautomata_la_SOURCES = \
	buffer.cpp \
    intermediate.pb.cc

AM_CPPFLAGS = \
	$(PROTOBUF_CPPFLAGS) \
	$(BOOST_CPPFLAGS) \
    -I$(srcdir)/include \
    -I$(builddir)/include
AM_LDFLAGS = \
    $(PROTOBUF_LDFLAGS) \
	$(BOOST_LDFLAGS) \
    -lprotobuf
    
# Ignore protobuf warnings.
CPPFLAGS += -Wno-shadow -Wno-extra

CLEANFILES = \
    intermediate.pb.cc \
    intermediate.pb.h \
    include/ironautomata/intermediate.pb.h

$(builddir)/include/ironautomata/intermediate.pb.h: intermediate.pb.h
	mkdir -p $(builddir)/include/ironautomata
	cp -p intermediate.pb.h $(builddir)/include/ironautomata

intermediate.pb.cc intermediate.pb.h: $(srcdir)/intermediate.proto
	protoc --cpp_out=$(builddir) --proto_path=$(srcdir) $<
