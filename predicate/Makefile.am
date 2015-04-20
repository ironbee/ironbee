include $(top_srcdir)/build/common.mk

SUBDIRS = tests

AM_LDFLAGS += \
    $(BOOST_LDFLAGS)              \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_thread$(BOOST_THREAD_SUFFIX) \
    -lboost_regex$(BOOST_SUFFIX)

module_LTLIBRARIES = \
	ibmod_predicate_constant.la \
	ibmod_predicate_core.la \
	ibmod_predicate_rules.la

noinst_LTLIBRARIES = libpredicate.la libpredicate_standard.la
libpredicate_la_SOURCES = \
    dag.cpp                  \
    eval.cpp                 \
    call_factory.cpp         \
    call_helpers.cpp         \
    parse.cpp                \
    dot.cpp                  \
    dot2.cpp                 \
    functional.cpp           \
    bfs.cpp                  \
    meta_call.cpp            \
    merge_graph.cpp          \
    reporter.cpp             \
    validate.cpp             \
    transform_graph.cpp      \
    validate_graph.cpp       \
    pre_eval_graph.cpp       \
    tree_copy.cpp            \
    value.cpp
libpredicate_standard_la_SOURCES = \
    standard.cpp             \
    standard_boolean.cpp     \
    standard_development.cpp \
    standard_filter.cpp      \
    standard_ironbee.cpp     \
    standard_template.cpp    \
    standard_list.cpp        \
    standard_math.cpp        \
    standard_predicate.cpp   \
    standard_string.cpp
noinst_PROGRAMS = pp_dot

EXTRA_DIST = \
	extract_predicate_from_waggle.rb \
	fmt_sexpr.rb \
	pp.rb \
	reference_index.rb \
	render.rb \
	render_ptrace.rb \
	ptrace.graffle \
	ptrace.pdf \
	introduction.html \
	introduction.adoc \
	lua_frontend.html \
	lua_frontend.adoc \
	reference.html \
	reference.adoc \
	profiling.html \
	profiling.adoc \
	reference_index.adoc \
	template.adoc \
	template.html \
	waggle_example.html \
	pp.graffle \
	pp.pdf \
	waggle_example.lua \
	guide.adoc \
	guide.html

ibmod_predicate_constant_la_SOURCES = ibmod_predicate_constant.cpp
if FREEBSD
ibmod_predicate_constant_la_LDFLAGS = $(AM_LDFLAGS) -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
ibmod_predicate_constant_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
endif

ibmod_predicate_core_la_SOURCES = ibmod_predicate_core.cpp
ibmod_predicate_core_la_LIBADD = libpredicate.la libpredicate_standard.la
if FREEBSD
ibmod_predicate_core_la_LDFLAGS = $(AM_LDFLAGS) -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
ibmod_predicate_core_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
endif

ibmod_predicate_rules_la_SOURCES = ibmod_predicate_rules.cpp
#ibmod_predicate_rules_la_LIBADD = libpredicate.la libpredicate_standard.la
if FREEBSD
ibmod_predicate_rules_la_LDFLAGS = $(AM_LDFLAGS) -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
ibmod_predicate_rules_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
endif

pp_dot_SOURCES = pp_dot.cpp
pp_dot_LDADD = \
    libpredicate_standard.la              \
    libpredicate.la                       \
    $(top_builddir)/ironbeepp/libibpp.la  \
    $(top_builddir)/util/libibutil.la     \
    $(top_builddir)/engine/libironbee.la
pp_dot_LDFLAGS = $(AM_LDFLAGS) -lboost_program_options$(BOOST_SUFFIX)

pp_dot_CPPFLAGS = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)

.PHONY: docs

DOCS = \
	$(srcdir)/reference.html \
	$(srcdir)/profiling.html \
	$(srcdir)/lua_frontend.html \
	$(srcdir)/introduction.html \
	$(srcdir)/guide.html \
	$(srcdir)/modules.html

docs: $(DOCS)

$(srcdir)/reference_index.adoc: $(srcdir)/reference.adoc $(srcdir)/reference_index.rb
	(cd $(srcdir) && $(RUBY) ./reference_index.rb $<)
$(srcdir)/reference.html: $(srcdir)/reference.adoc $(srcdir)/reference_index.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)

$(srcdir)/lua_frontend.html: $(srcdir)/lua_frontend.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)

$(srcdir)/introduction.html: $(srcdir)/introduction.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)

$(srcdir)/profiling.html: $(srcdir)/profiling.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)

$(srcdir)/guide.html: $(srcdir)/guide.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)

$(srcdir)/modules.html: $(srcdir)/modules.adoc
	test -z $(asciidoc) || (cd $(srcdir) && $(asciidoc) $<)
