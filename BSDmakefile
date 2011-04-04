# See LICENSE file for license and copyright information

all ${.TARGETS}:
	@if [ ! `which gmake` ] ; then \
		echo "zathura cannot be built with BSD make and GNU make cannot be found." \
			"Please install GNU make."; \
		exit 1; \
	fi
	@gmake $@
