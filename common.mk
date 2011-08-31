# See LICENSE file for license and copyright information

ifeq "$(VERBOSE)" "0"
ECHO=@echo
QUIET=@
else
ECHO=@\#
QUIET=
endif
