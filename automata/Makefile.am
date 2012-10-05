# If CPP is not defined, we do very litte.
lib_LTLIBRARIES = libiaeudoxus.la
libiaeudoxus_la_SOURCES = eudoxus.c eudoxus_automata.c \
	include/ironautomata/*.h
EXTRA_libiaeudoxus_la_SOURCES = eudoxus_subengine.c

AM_CPPFLAGS = -I$(srcdir)/include

if CPP

SUBDIRS = . bin tests

ACLOCAL_AMFLAGS = -I ../../acinclude

lib_LTLIBRARIES += libironautomata.la
libironautomata_la_SOURCES = \
	buffer.cpp \
    eudoxus_compiler.cpp \
    intermediate.cpp \
    intermediate_to_dot.cpp \
    logger.cpp \
    optimize_edges.cpp \
    translate_nonadvancing.cpp \
    include/ironautomata/*.hpp

nodist_libironautomata_la_SOURCES = \
    intermediate.pb.cc

LDADD = libironautomata.la libiaeudoxus.la
AM_CPPFLAGS += \
    -I$(builddir)/include \
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
    
# Ignore protobuf warnings.
CPPFLAGS += -Wno-shadow -Wno-extra

CLEANFILES = \
    intermediate.pb.cc \
    intermediate.pb.h \
    include/ironautomata/intermediate.pb.h

$(srcdir)/eudoxus_compiler.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/intermediate.cpp: $(builddir)/include/ironautomata/intermediate.pb.h

$(builddir)/include/ironautomata/intermediate.pb.h: intermediate.pb.h
	mkdir -p $(builddir)/include/ironautomata
	cp -p intermediate.pb.h $(builddir)/include/ironautomata

intermediate.pb.cc intermediate.pb.h: $(srcdir)/intermediate.proto
	protoc --cpp_out=$(builddir) --proto_path=$(srcdir) $<

endif