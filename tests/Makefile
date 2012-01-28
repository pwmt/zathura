# See LICENSE file for license and copyright information

include ../config.mk
include ../common.mk
include config.mk

PROJECT = tests
SOURCE  = tests.c $(wildcard test_*.c)
OBJECTS = ${SOURCE:.c=.o}

ZSOURCE   = $(shell find ../ -iname "*.c" -a ! -iname "database-*" ! -iname "main.c" ! -path "*tests*")
ZOBJECTS = ${ZSOURCE:.c=.o}

ifeq (${DATABASE}, sqlite)
ZSOURCE += ../database-sqlite.c
else
ifeq (${DATABASE}, plain)
ZSOURCE += ../database-plain.c
endif
endif

all: options ${PROJECT} run

run:
	$(QUIET)./${PROJECT}

options:
	@echo ${PROJECT} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "DFLAGS  = ${DFLAGS}"
	@echo "CC      = ${CC}"

%.o: %.c
	$(ECHO) CC $<
	@mkdir -p .depend
	$(QUIET)${CC} -c -I.. ${CPPFLAGS} ${CFLAGS} -o $@ $< -MMD -MF .depend/$@.dep

${PROJECT}: ${OBJECTS}
	$(ECHO) CC -o $@
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} -o $@ ${OBJECTS} ${ZOBJECTS} ${LIBS}

${OBJECTS}: ../config.mk

clean:
	$(QUIET)rm -rf ${OBJECTS} ${PROJECT}

.PHONY: all options clean debug

-include $(wildcard .depend/*.dep)
