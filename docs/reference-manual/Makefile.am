CLEANFILES = *.html
DOCVERSION = $(VERSION)

EXRTA_DIST = \
	ap-comparison-with-modsecurity.adoc \
	ap-configuration-examples.adoc \
	ap-todo.adoc \
	ch-diagnostics-devel-tools.adoc \
	ch-extending-ironbee.adoc \
	ch-inspection.adoc \
	ch-installation-guide.adoc \
	ch-introduction.adoc \
	ch-ironbee-configuration.adoc \
	ch-preface.adoc \
	ch-rule-reference.adoc \
	ch-server-configuration.adoc \
	config-examples/ \
	extending-ironbee/ \
	images/ \
	ironbee-reference-manual.adoc \
	ironbee-reference-manual.css

ref-manual: $(abs_builddir)/ironbee-reference-manual.html

$(abs_builddir)/ironbee-reference-manual.html: $(abs_top_srcdir)/VERSION $(abs_srcdir)/*.adoc
	@if test -n "$(ASCIIDOC)"; then \
		echo "Processing ironbee-reference-manual.adoc ..."; \
		(cd $(abs_srcdir) && $(ASCIIDOC) -a revnumber=$(DOCVERSION) $(ASCIIDOC_OPTIONS) -o $(abs_builddir)/ironbee-reference-manual.html ironbee-reference-manual.adoc && echo "Wrote $(abs_builddir)/ironbee-reference-manual.html"); \
	else \
		echo "You need asciidoc installed to build the reference manual."; \
	fi;

.PHONY: \
    ref-manual
