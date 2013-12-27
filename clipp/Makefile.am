include $(top_srcdir)/build/common.mk
bin_PROGRAMS=clipp

SUBDIRS = tests

# Avoid various warnings as error in clipp builds because protobuf is not
# careful about warnings.
#
# NOTE: this must be done in CPPFLAGS vs clipp_CPPFLAGS because
#       of option ordering.
CPPFLAGS += -Wno-shadow -Wno-extra

clipp_SOURCES = \
    clipp.cpp \
    configuration_parser.cpp \
    input.cpp \
    modsec_audit_log.cpp \
    modsec_audit_log_generator.cpp \
    view.cpp \
    ironbee.cpp \
    raw_generator.cpp \
    clipp.pb.cc \
    pb_consumer.cpp \
    htp_consumer.cpp \
    null_consumer.cpp \
    pb_generator.cpp \
    apache_generator.cpp \
    suricata_generator.cpp \
    htp_generator.cpp \
    echo_generator.cpp \
    connection_modifiers.cpp \
    parse_modifier.cpp \
    unparse_modifier.cpp \
    aggregate_modifier.cpp \
    edit_modifier.cpp \
    limit_modifier.cpp \
    select_modifier.cpp \
    header_modifiers.cpp \
    fill_body_modifier.cpp \
    split_modifier.cpp \
    time_modifier.cpp \
    aggregate_modifier.hpp \
    apache_generator.hpp \
    configuration_parser.hpp \
    connection_modifiers.hpp \
    control.hpp \
    echo_generator.hpp \
    edit_modifier.hpp \
    fill_body_modifier.hpp \
    header_modifiers.hpp \
    htp_consumer.hpp \
    htp_generator.hpp \
    input.hpp \
    ironbee.hpp \
    limit_modifier.hpp \
    modsec_audit_log_generator.hpp \
    modsec_audit_log.hpp \
    null_consumer.hpp \
    parse_modifier.hpp \
    pb_consumer.hpp \
    pb_generator.hpp \
    pcap_generator.hpp \
    random_support.hpp \
    raw_generator.hpp \
    select_modifier.hpp \
    split_modifier.hpp \
    suricata_generator.hpp \
    time_modifier.hpp \
    unparse_modifier.hpp \
    view.hpp

BUILT_SOURCES=clipp.pb.cc clipp.pb.h

EXTRA_DIST=clipp.proto \
    clipp_test.rb \
    clipp_test.rb \
    hash_to_pb.rb \
    json_to_pb.rb \
    pb_to_json.rb \
    clipp.pb.rb \
    tests


if HAVE_NIDS
clipp_SOURCES += pcap_generator.cpp
endif

clipp_CFLAGS = @PCRE_CFLAGS@ $(PROTOBUF_CFLAGS) $(NIDS_CFLAGS) $(NET_CFLAGS) $(GLIB_CFLAGS)
clipp_CPPFLAGS = @PCRE_CPPFLAGS@ $(AM_CPPFLAGS) \
    ${PROTOBUF_CPPFLAGS} \
    $(NIDS_CPPFLAGS) \
    $(NET_CPPFLAGS) \
    $(MODP_CPPFLAGS) \
    $(BOOST_CPPFLAGS)
clipp_LDFLAGS = \
    $(LDFLAGS) \
    $(AM_LDFLAGS) \
    $(PROTOBUF_LDFLAGS) \
    $(BOOST_LDFLAGS) \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_filesystem$(BOOST_SUFFIX) \
    -lboost_program_options$(BOOST_SUFFIX) \
    -lboost_regex$(BOOST_SUFFIX) \
    -lboost_thread$(BOOST_SUFFIX) \
    -lprotobuf \
    -lstdc++ \
    -lm \
    $(GLIB_LIBS)

if DARWIN
if ENABLE_LUA
# On Darwin (OSX) this is necessary for LuaJIT to run properly.
clipp_LDFLAGS += -pagezero_size 10000 -image_base 100000000
endif
endif

if HAVE_MODP
clipp_LDFLAGS += $(MODP_LDFLAGS) -lmodpbase64
endif

if HAVE_NIDS
clipp_LDFLAGS += $(NIDS_LDFLAGS) $(NET_LDFLAGS) -lnids -lnet -lpcap
endif

clipp_LDADD = \
    @PCRE_LDADD@ \
    $(top_builddir)/ironbeepp/libibpp.la \
    $(top_builddir)/engine/libironbee.la \
    $(top_builddir)/util/libibutil.la \
    $(top_builddir)/libs/libhtp/htp/libhtp.la \
    $(top_builddir)/automata/libiaeudoxus.la

CLEANFILES=clipp.pb.cc clipp.pb.h

$(builddir)/clipp.pb.cc $(builddir)/clipp.pb.h: $(srcdir)/clipp.proto
	protoc --cpp_out=$(builddir) --proto_path=$(srcdir) $<

.PHONY: build_fast_automata

build_fast_automata:
	(cd $(top_builddir)/fast; $(top_srcdir)/fast/build.rb $(top_srcdir)/clipp/tests/fast_rules.txt)
