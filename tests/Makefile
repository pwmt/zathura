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

ifneq ($(WITH_MAGIC),0)
INCS += $(MAGIC_INC)
LIBS += $(MAGIC_LIB)
CPPFLAGS += -DWITH_MAGIC
endif

ifneq ($(WITH_SYSTEM_SYNCTEX),0)
INCS   += $(SYNCTEX_INC)
LIBS   += $(SYNCTEX_LIB)
else
INCS   += $(ZLIB_INC)
LIBS   += $(ZLIB_LIB)
ZSOURCE += $(wildcard ../synctex/*.c)
CPPFLAGS += -I../synctex

ifeq (,$(findstring -DSYNCTEX_VERBOSE=0,${CPPFLAGS}))
CPPFLAGS += -DSYNCTEX_VERBOSE=0
endif
endif

ifeq (,$(findstring -Isynctex,${CPPFLAGS}))
CPPFLAGS += -Isynctex
endif
ifeq (,$(findstring -DSYNCTEX_VERBOSE=0,${CPPFLAGS}))
CPPFLAGS += -DSYNCTEX_VERBOSE=0
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
