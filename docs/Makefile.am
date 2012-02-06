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

doxygen-internal-pdf: doxygen_internal doxygen_internal.pdf

doxygen_internal.pdf: doxygen
	(cd doxygen_internal/latex && make refman.pdf) && cp doxygen/latex/refman.pdf doxygen_internal.pdf

doxygen-external-pdf: doxygen_external doxygen_external.pdf

doxygen_external.pdf: doxygen
	(cd doxygen_external/latex && make refman.pdf) && cp doxygen/latex/refman.pdf doxygen_external.pdf

doxygen: doxygen_internal doxygen_external

manual:
	@(cd docbook && $(MAKE) manual)

.PHONY: doxygen doxygen-internal-pdf doxygen-external-pdf manual
