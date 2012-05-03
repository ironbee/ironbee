include $(top_srcdir)/build/common.mk
bin_PROGRAMS=clipp

check-local:
	$(srcdir)/run_tests.rb \
		$(abs_builddir)/clipp \
		$(abs_builddir)/tests/ironbee.conf

clipp_SOURCES = \
    clipp.cpp \
    input.cpp \
    modsec_audit_log.cpp \
    modsec_audit_log_generator.cpp \
    raw_generator.cpp \
    ironbee_consumer.cpp \
    pb_consumer.cpp \
    pb_generator.cpp \
    clipp.pb.cc \
    view_consumer.cpp
clipp_LDFLAGS = \
    $(AM_LDFLAGS) \
    -lboost_system \
    -lboost_filesystem \
    -lboost_program_options \
    -lboost_regex \
    -lprotobuf
clipp_LDADD = \
    $(top_builddir)/ironbeepp/libibpp.la \
    $(top_builddir)/engine/libironbee.la
