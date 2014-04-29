# See LICENSE file for license and copyright information
#
include ../config.mk
include ../common.mk
include ../colors.mk
include config.mk

MAN_SOURCES=$(wildcard man/*.rst) $(wildcard man/*.txt) man/conf.py
DOXYGEN_SOURCES=$(wildcard ../*.h) Doxyfile
HTML_SORUCES=$(wildcard *.rst api/*.rst configuration/*.rst installation/*.rst usage/*.rst) conf.py

SPHINX_OPTS+=-d $(SPHINX_BUILDDIR)/doctrees

all: man html

clean:
	$(call colorecho,RM,doc/$(SPHINX_BUILDDIR))
	$(QUIET)rm -rf $(SPHINX_BUILDDIR)/

$(SPHINX_BUILDDIR)/html/index.html: $(HTML_SOURCES) $(SPHINX_BUILDDIR)/doxygen/xml/index.xml
	$(call colorecho,DOC,"Build HTML documentation")
	$(QUIET)$(SPHINX_BIN) -b html $(SPHINX_OPTS) . $(SPHINX_BUILDDIR)/html

$(SPHINX_BUILDDIR)/zathura.1 $(SPHINX_BUILDDIR)/zathurarc.5: $(MAN_SOURCES)
	$(call colorecho,DOC,"Build man pages")
	$(QUIET)$(SPHINX_BIN) -b man $(SPHINX_OPTS) man $(SPHINX_BUILDDIR)

$(SPHINX_BUILDDIR)/doxygen/xml/index.xml: $(DOXYGEN_SOURCES)
	$(QUIET)mkdir -p $(SPHINX_BUILDDIR)
	$(call colorecho,DOC,"Run doxygen")
	$(QUIET)$(DOXYGEN_BIN) Doxyfile

ifeq ($(shell which $(SPHINX_BIN) >/dev/null 2>&1; echo $$?), 1)
man:
html:
else
man: $(SPHINX_BUILDDIR)/zathura.1 $(SPHINX_BUILDDIR)/zathurarc.5

# TODO: Make a better test for breathe and sphinx_rtd_theme
ifeq ($(shell which $(DOXYGEN_BIN) >/dev/null 2>&1 && $(PYTHON_BIN) -c "import breathe; import sphinx_rtd_theme" >/dev/null 2>&1; echo $$?), 0)
html: $(SPHINX_BUILDDIR)/html/index.html
else
html:
endif
endif

.PHONY: clean html man all
