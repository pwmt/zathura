# See LICENSE file for license and copyright information

SUBDIRS = $(shell find -mindepth 2 -maxdepth 2 -iname "Makefile" -exec dirname '{}' \;)

all:
	@for dir in ${SUBDIRS}; do ${MAKE} -C $${dir}; done

debug:
	@for dir in ${SUBDIRS}; do ${MAKE} -C $${dir} debug; done

clean:
	@for dir in ${SUBDIRS}; do ${MAKE} -C $${dir} clean; done

install:
	@for dir in ${SUBDIRS}; do ${MAKE} -C $${dir} install; done

uninstall:
	@for dir in ${SUBDIRS}; do ${MAKE} -C $${dir} uninstall; done

.PHONY: all debug clean install uninstall
