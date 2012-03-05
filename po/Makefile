# See LICENSE file for license and copyright information

PROJECT = zathura
CATALOGS = $(wildcard *.po)
MOS = $(patsubst %.po, %/LC_MESSAGES/${PROJECT}.mo, $(CATALOGS))

include ../config.mk
include ../common.mk

all: ${MOS}

clean:
	rm -rf POTFILES.in POTFILES.in.tmp $(patsubst %.po, %, $(CATALOGS)) ${PROJECT}.pot

POTFILES.in: $(wildcard ../*.c)
	$(QUIET) set -e && rm -f $@.tmp && touch $@.tmp && \
	for f in $(^F) ; do \
		echo $$f >> $@.tmp ; \
	done && \
	mv $@.tmp $@

${PROJECT}.pot: POTFILES.in
	$(ECHO) regenerate ${PROJECT}.pot
	$(QUIET)intltool-update --pot --gettext-package=${PROJECT}

%.po: ${PROJECT}.pot
	$(ECHO) updating language $(shell echo $@ | sed 's/\.po//')
	$(QUIET)intltool-update --dist --gettext-package=${PROJECT} $(shell echo $@ | sed 's/\.po//')

%/LC_MESSAGES/${PROJECT}.mo: %.po
	@mkdir -p $(@D)
	$(ECHO) msgfmt $(shell echo $< | sed 's/\.po//')
	$(QUIET)msgfmt $< -o $@ -f

install: ${MOS}
	$(ECHO) installing translations
	$(QUIET)set -e && for f in $^ ; do \
		mkdir -p $(DESTDIR)$(LOCALEDIR)/`dirname $$f`; \
		install -m 644 $$f $(DESTDIR)$(LOCALEDIR)/`dirname $$f` ; \
	done
