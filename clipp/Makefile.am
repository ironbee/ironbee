include $(top_srcdir)/build/common.mk

SUBDIRS = tests

# Avoid various warnings as error in clipp builds because protobuf is not
# careful about warnings.
#
# NOTE: this must be done in CPPFLAGS vs clipp_CPPFLAGS because
#       of option ordering.
CPPFLAGS += -Wno-shadow -Wno-extra
clipp_CPPFLAGS = $(AM_CPPFLAGS)

lib_LTLIBRARIES = libclipp.la
bin_PROGRAMS=clipp

libclipp_la_SOURCES = \
    aggregate_modifier.cpp \
    aggregate_modifier.hpp \
    apache_generator.cpp \
    apache_generator.hpp \
    burp_generator.hpp \
    clipp.pb.cc \
    configuration_parser.cpp \
    configuration_parser.hpp \
    connection_modifiers.cpp \
    connection_modifiers.hpp \
    control.hpp \
    echo_generator.cpp \
    echo_generator.hpp \
    edit_modifier.cpp \
    edit_modifier.hpp \
    fill_body_modifier.cpp \
    fill_body_modifier.hpp \
    header_modifiers.cpp \
    header_modifiers.hpp \
    htp_consumer.cpp \
    htp_consumer.hpp \
    htp_generator.cpp \
    htp_generator.hpp \
    input.cpp \
    input.hpp \
    ironbee.cpp \
    ironbee.hpp \
    limit_modifier.cpp \
    limit_modifier.hpp \
    modsec_audit_log.cpp \
    modsec_audit_log.hpp \
    modsec_audit_log_generator.cpp \
    modsec_audit_log_generator.hpp \
    null_consumer.cpp \
    null_consumer.hpp \
    parse_modifier.cpp \
    parse_modifier.hpp \
    pb_consumer.cpp \
    pb_consumer.hpp \
    pb_generator.cpp \
    pb_generator.hpp \
    pcap_generator.hpp \
    random_support.hpp \
    raw_consumer.cpp \
    raw_consumer.hpp \
    raw_generator.cpp \
    raw_generator.hpp \
    select_modifier.cpp \
    select_modifier.hpp \
    split_modifier.cpp \
    split_modifier.hpp \
    suricata_generator.cpp \
    suricata_generator.hpp \
    time_modifier.cpp \
    time_modifier.hpp \
    unparse_modifier.cpp \
    unparse_modifier.hpp \
    view.cpp \
    view.hpp \
    proxy.cpp \
    proxy.hpp

clipp_SOURCES = \
    clipp.cpp

clippruby_SCRIPTS = \
    clipp_test.rb \
    hash_to_pb.rb \
    json_to_pb.rb \
    pb_to_json.rb \
    clipp.pb.rb \
    clippscript.rb \
    ibaudit_to_pb.rb \
    ibtxlog_to_pb.rb \
    ironbee.config.erb

BUILT_SOURCES = clipp.pb.cc clipp.pb.h

EXTRA_DIST= \
    clipp.proto \
    clipp_test.rb \
    clipp_test.rb \
    hash_to_pb.rb \
    json_to_pb.rb \
    pb_to_json.rb \
    clipp.pb.rb \
    clipp.html \
    clipp.adoc \
    clippdev.html \
    clippdev.adoc \
    clippscript.html \
    clippscript.adoc \
    clippscript.rb \
    ibaudit_to_pb.rb \
    ibtxlog_to_pb.rb \
    ironbee.config.erb

.PHONY: docs

DOCS = \
    $(srcdir)/clipp.html \
    $(srcdir)/clippdev.html \
    $(srcdir)/clippscript.html


docs: $(DOCS)

$(srcdir)/clipp.html: $(srcdir)/clipp.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)

$(srcdir)/clippdev.html: $(srcdir)/clippdev.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)

$(srcdir)/clippscript.html: $(srcdir)/clippscript.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)

if HAVE_NIDS
clipp_SOURCES += pcap_generator.cpp
endif

if HAVE_LIBXML2
clipp_SOURCES += burp_generator.cpp
clipp_CPPFLAGS += -I$(top_srcdir)/libs/stringencoders/src
endif

libclipp_la_CPPFLAGS = @PCRE_CPPFLAGS@ $(AM_CPPFLAGS) \
    ${PROTOBUF_CPPFLAGS} \
    $(NIDS_CPPFLAGS) \
    $(NET_CPPFLAGS) \
    $(BOOST_CPPFLAGS) \
    -I$(top_srcdir)/libs/stringencoders/src
libclipp_la_LDFLAGS = \
    $(LDFLAGS) \
    $(AM_LDFLAGS) \
    $(PROTOBUF_LDFLAGS) \
    $(BOOST_LDFLAGS) \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_filesystem$(BOOST_SUFFIX) \
    -lboost_program_options$(BOOST_SUFFIX) \
    -lboost_regex$(BOOST_SUFFIX) \
    -lboost_thread$(BOOST_THREAD_SUFFIX) \
    -lprotobuf
libclipp_la_LIBADD = \
    $(abs_top_builddir)/libs/stringencoders/src/libmodpbase64.la

clipp_LDFLAGS = \
    $(LDFLAGS) \
    $(AM_LDFLAGS) \
    $(PROTOBUF_LDFLAGS) \
    $(BOOST_LDFLAGS) \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_filesystem$(BOOST_SUFFIX) \
    -lboost_program_options$(BOOST_SUFFIX) \
    -lboost_regex$(BOOST_SUFFIX) \
    -lboost_thread$(BOOST_THREAD_SUFFIX) \
    -lprotobuf \
    -lstdc++ \
    -lm

if HAVE_NIDS
clipp_LDFLAGS += $(NIDS_LDFLAGS) $(NET_LDFLAGS) -lnids -lnet -lpcap
endif

if HAVE_LIBXML2
clipp_LDFLAGS += $(LIBXML2_LDFLAGS) -lxml2
clipp_CPPFLAGS += $(LIBXML2_CFLAGS)
endif

clipp_LDADD = \
    libclipp.la \
    @PCRE_LDADD@ \
    $(top_builddir)/ironbeepp/libibpp.la \
    $(top_builddir)/engine/libironbee.la \
    $(top_builddir)/util/libibutil.la \
    $(top_builddir)/libs/libhtp/htp/libhtp.la \
    $(top_builddir)/automata/libiaeudoxus.la \
    $(abs_top_builddir)/libs/stringencoders/src/libmodpbase64.la

if DARWIN
# On Darwin (OSX) this is necessary for LuaJIT to run properly.
clipp_LDFLAGS += -pagezero_size 10000 -image_base 100000000
endif

CLEANFILES=clipp.pb.cc clipp.pb.h

$(builddir)/clipp.pb.cc $(builddir)/clipp.pb.h: $(srcdir)/clipp.proto
	protoc --cpp_out=$(builddir) --proto_path=$(srcdir) $<

.PHONY: build_fast_automata

build_fast_automata:
	(cd $(top_builddir)/fast; $(RUBY) $(top_srcdir)/fast/build.rb $(top_srcdir)/clipp/tests/fast_rules.txt)
