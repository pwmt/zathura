# See LICENSE file for license and copyright information
#
include ../config.mk
include ../common.mk
include ../colors.mk
include config.mk

ifeq ($(shell which $(SPHINX_BIN) >/dev/null 2>&1; echo $$?), 1)
	$(error The '$(SPHINX_BIN)' command was not found.)
endif

all: html man

init:
	$(QUIET)mkdir -p $(SPHINX_BUILDDIR)/

clean:
	$(call colorecho,RM,doc/$(SPHINX_BUILDDIR))
	$(QUIET)rm -rf $(SPHINX_BUILDDIR)/

html: doxygen
	$(call colorecho,DOC,"Build HTML documentation")
	$(QUIET)$(SPHINX_BIN) -b html $(SPHINX_OPTS) $(SPHINX_BUILDDIR)/html

man: doxygen
	$(call colorecho,DOC,"Build man pages")
	$(QUIET)$(SPHINX_BIN) -b man $(SPHINX_OPTS) $(SPHINX_BUILDDIR)/man

doxygen: init
	$(call colorecho,DOC,"Run doxygen")
	$(QUIET)$(DOXYGEN_BIN) Doxyfile

.PHONY: clean html man
