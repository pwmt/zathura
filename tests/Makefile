# See LICENSE file for license and copyright information

include ../config.mk
include ../common.mk
include config.mk

PROJECT = tests
SOURCE  = tests.c $(wildcard test_*.c)
OBJECTS = ${SOURCE:.c=.o}

ZSOURCE   = $(shell find ../ -maxdepth 1 -iname "*.c" -a ! -iname "database-*" ! -iname "main.c")
ZOBJECTS = ${ZSOURCE:.c=.o}

ifeq (${DATABASE}, sqlite)
INCS   += $(SQLITE_INC)
LIBS   += $(SQLITE_LIB)
ZSOURCE += ../database-sqlite.c
else
ifeq (${DATABASE}, plain)
ZSOURCE += ../database-plain.c
endif
endif

all: ${PROJECT}

run: ${PROJECT}
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

${PROJECT}: options ${OBJECTS}
	$(QUIET)make -C ..
	$(ECHO) CC -o $@
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} -o $@ ${OBJECTS} ${ZOBJECTS} ${LIBS}

${OBJECTS}: ../config.mk

clean:
	$(QUIET)rm -rf ${OBJECTS} ${PROJECT} *.gcno *.gcda .depend

.PHONY: all options clean debug run

-include $(wildcard .depend/*.dep)
