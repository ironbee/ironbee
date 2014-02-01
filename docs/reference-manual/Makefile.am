CLEANFILES = *.html
DOCVERSION = $(VERSION)

# TODO: Add detection to autoconf
ASCIIDOC = asciidoc

ironbee-reference-manual.html: $(abs_srcdir)/*.adoc
	@if test -n "$(ASCIIDOC)"; then \
		echo "Processing ironbee-reference-manual.adoc ..."; \
		(cd $(abs_srcdir) && $(ASCIIDOC) -a revnumber=$(DOCVERSION) -o $(abs_builddir)/ironbee-reference-manual.html ironbee-reference-manual.adoc && echo "Wrote $(abs_builddir)/ironbee-reference-manual.html"); \
	else \
		echo "You need asciidoc installed to build the reference manual."; \
	fi;
