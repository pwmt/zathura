# See LICENSE file for license and copyright information

include ../config.mk
include ../common.mk
include config.mk

PROJECT = tests
SOURCE  = tests.c $(wildcard test_*.c)
OBJECTS = ${SOURCE:.c=.o}

ZOSOURCE   = $(filter-out ../main.c,$(wildcard ../*.c))

ifneq (${WITH_SQLITE},0)
INCS   += $(SQLITE_INC)
LIBS   += $(SQLITE_LIB)
ZSOURCE += $(ZOSOURCE)
ifeq (,$(findstring -DWITH_SQLITE,${CPPFLAGS}))
CPPFLAGS += -DWITH_SQLITE
endif
else
ZSOURCE = $(filter-out ../database-sqlite.c,$(ZOSOURCE))
endif

ZOBJECTS = ${ZSOURCE:.c=.o}

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
