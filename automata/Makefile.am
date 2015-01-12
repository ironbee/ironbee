
include $(top_srcdir)/build/common.mk

# If CPP is not defined, we do very litte.
lib_LTLIBRARIES = libiaeudoxus.la
libiaeudoxus_la_SOURCES = eudoxus.c eudoxus_automata.c
libiaeudoxus_la_LDFLAGS = $(AM_LDFLAGS) \
    -version-info @LIBRARY_VERSION_INFO@ \
    -release @LIBRARY_RELEASE@
ironautomata_include_HEADERS = \
    $(srcdir)/include/ironautomata/apidoc.h \
    $(srcdir)/include/ironautomata/bits.h \
    $(srcdir)/include/ironautomata/eudoxus.h \
    $(srcdir)/include/ironautomata/eudoxus_automata.h \
    $(srcdir)/include/ironautomata/eudoxus_subautomata.h \
    $(srcdir)/include/ironautomata/vls.h
EXTRA_libiaeudoxus_la_SOURCES = eudoxus_subengine.c

AM_CPPFLAGS += -I$(srcdir)/include
CFLAGS += -Wno-inline

if CPP

SUBDIRS = . bin tests

ACLOCAL_AMFLAGS = -I ../../acinclude

lib_LTLIBRARIES += libironautomata.la
libironautomata_la_SOURCES = \
	buffer.cpp \
    deduplicate_outputs.cpp \
    eudoxus_compiler.cpp \
    intermediate.cpp \
    intermediate_to_dot.cpp \
    logger.cpp \
    optimize_edges.cpp \
    translate_nonadvancing.cpp \
    aho_corasick.cpp
libironautomata_la_LDFLAGS = $(AM_LDFLAGS) \
    -lprotobuf \
    -version-info @LIBRARY_VERSION_INFO@ \
    -release @LIBRARY_RELEASE@

ironautomata_include_HEADERS += \
    $(srcdir)/include/ironautomata/buffer.hpp \
    $(srcdir)/include/ironautomata/deduplicate_outputs.hpp \
    $(srcdir)/include/ironautomata/eudoxus_compiler.hpp \
    $(srcdir)/include/ironautomata/intermediate.hpp \
    $(srcdir)/include/ironautomata/intermediate_to_dot.hpp \
    $(srcdir)/include/ironautomata/logger.hpp \
    $(srcdir)/include/ironautomata/optimize_edges.hpp \
    $(srcdir)/include/ironautomata/translate_nonadvancing.hpp

nodist_ironautomata_include_HEADERS = \
    $(builddir)/include/ironautomata/intermediate.pb.h

ironautomata_generator_include_HEADERS = \
    $(srcdir)/include/ironautomata/generator/aho_corasick.hpp

nodist_libironautomata_la_SOURCES = intermediate.pb.cc

BUILT_SOURCES = intermediate.pb.cc \
    $(builddir)/include/ironautomata/intermediate.pb.h
EXTRA_DIST = \
	intermediate.proto \
	doc \
	intermediate.pb.rb \
	intermediate.rb

AM_CPPFLAGS += \
    -I$(builddir)/include \
	$(PROTOBUF_CPPFLAGS) \
	$(BOOST_CPPFLAGS)
AM_LDFLAGS += \
    $(PROTOBUF_LDFLAGS) \
	$(BOOST_LDFLAGS) \
    -lboost_program_options$(BOOST_SUFFIX) \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_filesystem$(BOOST_SUFFIX) \
    -lboost_chrono$(BOOST_SUFFIX)

# Ignore protobuf warnings.
CPPFLAGS += -Wno-shadow -Wno-extra

CLEANFILES = \
    intermediate.pb.cc \
    intermediate.pb.h \
    include/ironautomata/intermediate.pb.h

$(srcdir)/include/ironautomata/intermediate.hpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/eudoxus_compiler.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/intermediate.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/deduplicate_outputs.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/ac_generator.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/optimize.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/trie_generator.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/intermediate_to_dot.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/optimize_edges.cpp: $(builddir)/include/ironautomata/intermediate.pb.h
$(srcdir)/translate_nonadvancing.cpp: $(builddir)/include/ironautomata/intermediate.pb.h

$(builddir)/include/ironautomata/intermediate.pb.h: intermediate.pb.h
	mkdir -p $(builddir)/include/ironautomata
	cp -p intermediate.pb.h $(builddir)/include/ironautomata

intermediate.pb.cc intermediate.pb.h: $(srcdir)/intermediate.proto
	protoc --cpp_out=$(builddir) --proto_path=$(srcdir) $<

endif
