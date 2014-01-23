SUBDIRS = docbook
CLEANFILES = *.pdf

clean-local:
	rm -rf doxygen_internal doxygen_external

doxygen_internal: internal.doxy $(DOXYGEN_DEPS)
	@if test -n "$(DOXYGEN)"; then \
		$(DOXYGEN) $<; \
	else \
		echo "You need doxygen installed to build the docs."; \
	fi;

doxygen_external: external.doxy $(DOXYGEN_DEPS)
	@if test -n "$(DOXYGEN)"; then \
		$(DOXYGEN) $<; \
	else \
		echo "You need doxygen installed to build the docs."; \
	fi;

doxygen_internal.pdf: doxygen_internal
	(cd doxygen_internal/latex && make refman.pdf) && cp doxygen/latex/refman.pdf doxygen_internal.pdf

doxygen_external.pdf: doxygen_external
	(cd doxygen_external/latex && make refman.pdf) && cp doxygen/latex/refman.pdf doxygen_external.pdf

doxygen: doxygen_internal doxygen_external

manual:
	@(cd docbook && $(MAKE) manual)

ref-manual:
	@(cd reference-manual && $(MAKE) ironbee-reference-manual.html)

.PHONY: \
    doxygen \
    doxygen_internal \
    doxygen_external \
    manual \
    ref-manual
