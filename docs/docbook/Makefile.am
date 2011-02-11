SUBDIRS = manual

clean-local:
	rm -rf output

manual:
	@(cd manual && $(MAKE) output)

.PHONY: doxygen-pdf manual
