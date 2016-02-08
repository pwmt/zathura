# See LICENSE file for license and copyright information

include ../config.mk
include ../common.mk
include ../colors.mk

PROJECT  = zathura
CATALOGS = $(sort $(wildcard *.po))
LINGUAS ?= $(patsubst %.po, %, $(CATALOGS))
ifeq ($(LINGUAS),)
ALINGUAS =
else
ALINGUAS = $(shell find $(patsubst %, %.po, $(LINGUAS)) 2>/dev/null)
endif
MOS      = $(patsubst %, %/LC_MESSAGES/${GETTEXT_PACKAGE}.mo, $(patsubst %.po, %, $(ALINGUAS)))

all: ${MOS}

clean:
	$(QUIET)rm -rf POTFILES.in POTFILES.in.tmp $(patsubst %.po, %, $(CATALOGS)) ${PROJECT}.pot

POTFILES.in: $(sort $(wildcard ../zathura/*.c))
	$(QUIET) set -e && rm -f $@.tmp && touch $@.tmp && \
	for f in $(^F) ; do \
		echo zathura/$$f >> $@.tmp ; \
	done && \
	mv $@.tmp $@

${GETTEXT_PACKAGE}.pot: POTFILES.in
	$(ECHO) regenerate $@
	$(QUIET)intltool-update --pot --gettext-package=${GETTEXT_PACKAGE}

update-po: ${GETTEXT_PACKAGE}.pot
	$(call colorecho,PO,"Updating po files ${CATALOGS}")
	$(QUIET)set -e && for f in ${CATALOGS} ; do \
		intltool-update --dist --gettext-package=${GETTEXT_PACKAGE} `echo $$f | sed 's/\.po//'` ; \
		sed -i 's/Report-Msgid-Bugs-To: \\n/Report-Msgid-Bugs-To: http:\/\/bugs.pwmt.org\\n/'  "$$f" ; \
	done

%/LC_MESSAGES/${GETTEXT_PACKAGE}.mo: %.po
	@mkdir -p $(@D)
	$(call colorecho,MSGFMT,$(shell echo $< | sed 's/\.po//'))
	$(QUIET)${MSGFMT} ${MSGFMTFLAGS} -c $< -o $@

install: ${MOS}
	$(call colorecho,INSTALL,"Install translations")
	$(QUIET)set -e && for f in $^ ; do \
		mkdir -p -m 755 $(DESTDIR)$(LOCALEDIR)/`dirname $$f`; \
		install -m 644 $$f $(DESTDIR)$(LOCALEDIR)/`dirname $$f` ; \
	done

uninstall: ${MOS}
	$(call colorecho,UNINSTALL,"Uninstall translations")
	$(QUIET)set -e && for f in $^ ; do \
		rm -f $(LOCALEDIR)/$$f; \
	done

.PHONY: all clean install uninstall update-po
