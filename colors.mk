# See LICENSE file for license and copyright information
#

TPUT ?= /usr/bin/tput
TPUT_AVAILABLE ?= $(shell ${TPUT} -V 2>/dev/null)

ifdef TPUT_AVAILABLE
ifeq ($(COLOR),1)
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
