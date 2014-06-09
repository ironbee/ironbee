ACLOCAL_AMFLAGS = -I ../../acinclude

if CPP

noinst_PROGRAMS = generate

EXTRA_DIST = \
	build.rb \
	extract.rb \
	extract_waggle.rb \
	re_to_ac.rb \
	suggest.rb \
	test_re_to_ac.rb \
	fast.html \
	fast.md

LDADD = ../automata/libironautomata.la ../automata/libiaeudoxus.la

AM_CPPFLAGS += \
    -I$(srcdir)/../automata/include \
    -I$(builddir)/../automata/include \
	$(PROTOBUF_CPPFLAGS) \
	$(BOOST_CPPFLAGS)
AM_LDFLAGS += \
    $(PROTOBUF_LDFLAGS) \
	$(BOOST_LDFLAGS) \
    -lprotobuf \
    -lboost_program_options$(BOOST_SUFFIX) \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_filesystem$(BOOST_SUFFIX) \
    -lboost_chrono$(BOOST_SUFFIX)

generate_SOURCES = generate.cpp

endif
