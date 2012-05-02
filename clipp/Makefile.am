include $(top_srcdir)/build/common.mk
bin_PROGRAMS=clipp

clipp_SOURCES = \
    clipp.cpp \
    input.cpp \
    modsec_audit_log.cpp \
    modsec_audit_log_generator.cpp \
    raw_generator.cpp
clipp_LDFLAGS = \
    $(AM_LDFLAGS) \
    -lboost_system \
    -lboost_filesystem \
    -lboost_program_options \
    -lboost_regex
clipp_LDADD = \
    $(top_builddir)/engine/libironbee.la \
    $(top_builddir)/ironbeepp/libibpp.la
