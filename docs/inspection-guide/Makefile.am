CLEANFILES = *.html
DOCVERSION = $(VERSION)

EXTRA_DIST = \
	$(subst $(srcdir)/,,$(wildcard $(srcdir)/*.adoc)) \
	*.pdf \
	images/ \
	ironbee-inspection-guide.css

IMAGES = \
	$(subst $(abs_srcdir),$(abs_builddir),$(wildcard $(abs_srcdir)/images/*.png))

guide: $(abs_builddir)/ironbee-inspection-guide.html

$(abs_builddir)/ironbee-inspection-guide.html: $(abs_top_srcdir)/VERSION $(abs_srcdir)/*.adoc $(IMAGES)
	@if test -n "$(ASCIIDOC)"; then \
		echo "Processing ironbee-inspection-guide.adoc ..."; \
		(cd $(abs_srcdir) && $(ASCIIDOC) -a revnumber=$(DOCVERSION) $(ASCIIDOC_OPTIONS) -o $(abs_builddir)/ironbee-inspection-guide.html ironbee-inspection-guide.adoc && echo "Wrote $(abs_builddir)/ironbee-inspection-guide.html"); \
	else \
		echo "You need asciidoc installed to build the inspection guide."; \
	fi;

$(IMAGES): $(abs_builddir)/images/%.png: $(abs_srcdir)/images/%.png
	$(INSTALL) -d images
	if [ x"$<" != x"$@" ]; then \
		$(INSTALL) -C $< $@; \
	fi

.PHONY: \
    guide
