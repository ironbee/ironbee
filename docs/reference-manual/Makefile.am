CLEANFILES = *.html
ASCIIDOC = asciidoc

ironbee-reference-manual.html: $(abs_srcdir)/*.adoc
	@if test -n "$(ASCIIDOC)"; then \
		(cd $(abs_srcdir) && $(ASCIIDOC) -o $(abs_builddir)/ironbee-reference-manual.html ironbee-reference-manual.adoc); \
	else \
		echo "You need asciidoc installed to build the reference manual."; \
	fi;
