# See LICENSE file for license and copyright information

ifeq ($(COLOR),1)
# GCC diagnostics colors
DIAGNOSTICS_COLOR_AVAILABLE ?= $(shell ($(CC) -fdiagnostics-color=always -E - </dev/null >/dev/null 2>/dev/null && echo 1) || echo 0)
ifeq ($(DIAGNOSTICS_COLOR_AVAILABLE),1)
CPPFLAGS     += -fdiagnostics-color=always
endif

# colorful output
TPUT ?= /usr/bin/tput
TPUT_AVAILABLE ?= $(shell ${TPUT} -V 2>/dev/null)

ifdef TPUT_AVAILABLE
COLOR_NORMAL  = `$(TPUT) sgr0`
COLOR_ACTION  = `$(TPUT) bold``$(TPUT) setaf 3`
COLOR_COMMENT = `$(TPUT) bold``$(TPUT) setaf 2`
COLOR_BRACKET = `$(TPUT) setaf 4`
define colorecho
	@echo $(COLOR_BRACKET)" ["$(COLOR_ACTION)$1$(COLOR_BRACKET)"] "$(COLOR_COMMENT)$2$(COLOR_BRACKET) $(COLOR_NORMAL)
endef
else
define colorecho
	@echo " [$1]" $2
endef
endif
else
define colorecho
	@echo " [$1]" $2
endef
endif
