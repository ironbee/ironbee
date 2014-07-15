CLEANFILES = *.html
DOCVERSION = $(VERSION)

EXTRA_DIST = \
	$(subst $(srcdir)/,,$(wildcard $(srcdir)/*.adoc)) \
	$(subst $(srcdir)/,,$(wildcard $(srcdir)/*.rb)) \
	config-examples/ \
	extending-ironbee/ \
	images/ \
	ironbee-reference-manual.css


ref-manual: $(abs_builddir)/ironbee-reference-manual.html  $(abs_srcdir)/modules.adoc

$(abs_builddir)/ironbee-reference-manual.html: $(abs_top_srcdir)/VERSION $(abs_srcdir)/*.adoc
	@if test -n "$(ASCIIDOC)"; then \
		echo "Processing ironbee-reference-manual.adoc ..."; \
		(cd $(abs_srcdir) && $(ASCIIDOC) -a revnumber=$(DOCVERSION) $(ASCIIDOC_OPTIONS) -o $(abs_builddir)/ironbee-reference-manual.html ironbee-reference-manual.adoc && echo "Wrote $(abs_builddir)/ironbee-reference-manual.html"); \
	else \
		echo "You need asciidoc installed to build the reference manual."; \
	fi;

$(abs_srcdir)/modules.adoc: $(abs_srcdir)/module-*.adoc $(abs_srcdir)/generate-module-index.rb $(abs_srcdir)/generate_modules.rb
	(cd $(abs_srcdir); ./generate-module-index.rb)
	(cd $(abs_srcdir); ./generate_modules.rb)

.PHONY: \
    ref-manual
