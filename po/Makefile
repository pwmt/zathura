# See LICENSE file for license and copyright information

PROJECT = zathura
GETTEXT_PACKAGE = $(PROJECT)
CATALOGS  = $(wildcard *.po)
LINGUAS  ?= $(patsubst %.po, %, $(CATALOGS))
ifeq ($(LINGUAS),)
ALINGUAS =
else
ALINGUAS = $(shell find $(patsubst %, %.po, $(LINGUAS)) 2>/dev/null)
endif
MOS = $(patsubst %, %/LC_MESSAGES/${GETTEXT_PACKAGE}.mo, $(patsubst %.po, %, $(ALINGUAS)))

include ../config.mk
include ../common.mk

all: ${MOS}

clean:
	$(QUIET)rm -rf POTFILES.in POTFILES.in.tmp $(patsubst %.po, %, $(CATALOGS)) ${PROJECT}.pot

POTFILES.in: $(wildcard ../zathura/*.c)
	$(QUIET) set -e && rm -f $@.tmp && touch $@.tmp && \
	for f in $(^F) ; do \
		echo zathura/$$f >> $@.tmp ; \
	done && \
	mv $@.tmp $@

${PROJECT}.pot: POTFILES.in
	$(ECHO) regenerate ${PROJECT}.pot
	$(QUIET)intltool-update --pot --gettext-package=${PROJECT}

update-po: ${PROJECT}.pot
	$(ECHO) updating po: files ${CATALOGS}
	$(QUIET)set -e && for f in ${CATALOGS} ; do \
		intltool-update --dist --gettext-package=${PROJECT} `echo $$f | sed 's/\.po//'` ; \
		sed -i 's/Report-Msgid-Bugs-To: \\n/Report-Msgid-Bugs-To: http:\/\/bugs.pwmt.org\\n/'  "$$f" ; \
	done

%/LC_MESSAGES/${PROJECT}.mo: %.po
	@mkdir -p $(@D)
	$(ECHO) msgfmt $(shell echo $< | sed 's/\.po//')
	$(QUIET)${MSGFMT} ${MSGFMTFLAGS} -c $< -o $@

install: ${MOS}
	$(ECHO) installing translations
	$(QUIET)set -e && for f in $^ ; do \
		mkdir -p -m 755 $(DESTDIR)$(LOCALEDIR)/`dirname $$f`; \
		install -m 644 $$f $(DESTDIR)$(LOCALEDIR)/`dirname $$f` ; \
	done

uninstall: ${MOS}
	$(ECHO) removing translations
	$(QUIET)set -e && for f in $^ ; do \
		rm -f $(LOCALEDIR)/$$f; \
	done

.PHONY: all clean update-po install uninstall
