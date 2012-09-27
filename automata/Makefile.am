SUBDIRS = . tests

ACLOCAL_AMFLAGS = -I ../../acinclude

lib_LTLIBRARIES = libironautomata.la
libironautomata_la_SOURCES = \
	buffer.cpp \
    eudoxus.c \
    eudoxus_automata.c \
    eudoxus_compiler.cpp \
    intermediate.cpp \
    logger.cpp \
    intermediate.pb.cc

bin_PROGRAMS = \
    bench \
    ec \
    intermediate_to_dot
  
bench_SOURCES = bench.cpp
ec_SOURCES = ec.cpp
intermediate_to_dot_SOURCES = intermediate_to_dot.cpp

LDADD = libironautomata.la
AM_CPPFLAGS = \
	$(PROTOBUF_CPPFLAGS) \
	$(BOOST_CPPFLAGS) \
    -I$(srcdir)/include \
    -I$(builddir)/include
AM_LDFLAGS = \
    $(PROTOBUF_LDFLAGS) \
	$(BOOST_LDFLAGS) \
    -lprotobuf \
    -lboost_program_options \
    -lboost_system \
    -lboost_filesystem \
    -lboost_chrono
    
# Ignore protobuf warnings.
CPPFLAGS += -Wno-shadow -Wno-extra

CLEANFILES = \
    intermediate.pb.cc \
    intermediate.pb.h \
    include/ironautomata/intermediate.pb.h

$(srcdir)/intermediate.cpp: $(builddir)/include/ironautomata/intermediate.pb.h

$(builddir)/include/ironautomata/intermediate.pb.h: intermediate.pb.h
	mkdir -p $(builddir)/include/ironautomata
	cp -p intermediate.pb.h $(builddir)/include/ironautomata

intermediate.pb.cc intermediate.pb.h: $(srcdir)/intermediate.proto
	protoc --cpp_out=$(builddir) --proto_path=$(srcdir) $<
