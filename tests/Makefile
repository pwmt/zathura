# See LICENSE file for license and copyright information

include ../config.mk
include ../colors.mk
include ../common.mk

include config.mk

PROJECT = tests
SOURCE  = tests.c $(wildcard test_*.c)
OBJECTS       = $(addprefix ${BUILDDIR_RELEASE}/,${SOURCE:.c=.o})
OBJECTS_DEBUG = $(addprefix ${BUILDDIR_DEBUG}/,${SOURCE:.c=.o})
OBJECTS_GCOV  = $(addprefix ${BUILDDIR_GCOV}/,${SOURCE:.c=.o})

ZATHURA_OBJECTS =  \
	$(filter-out ../${BUILDDIR_RELEASE}/zathura/main.o, $(wildcard ../${BUILDDIR_RELEASE}/zathura/*.o))
ZATHURA_OBJECTS_DEBUG =  \
	$(filter-out ../${BUILDDIR_DEBUG}/zathura/main.o, $(wildcard ../${BUILDDIR_DEBUG}/zathura/*.o))
ZATHURA_OBJECTS_GCOV=  \
	$(filter-out ../${BUILDDIR_GCOV}/zathura/main.o, $(wildcard ../${BUILDDIR_GCOV}/zathura/*.o))

ifneq (${WITH_SQLITE},0)
INCS     += $(SQLITE_INC)
LIBS     += $(SQLITE_LIB)
CPPFLAGS += -DWITH_SQLITE
else
SOURCE = $(filter-out database-sqlite.c,$(OSOURCE))
endif

ifneq ($(WITH_MAGIC),0)
INCS     += $(MAGIC_INC)
LIBS     += $(MAGIC_LIB)
CPPFLAGS += -DWITH_MAGIC
endif

ifneq ($(WITH_SYSTEM_SYNCTEX),0)
else
ZATHURA_OBJECTS       += $(wildcard ../${BUILDDIR_RELEASE}/zathura/synctex/*.o)
ZATHURA_OBJECTS_DEBUG += $(wildcard ../${BUILDDIR_DEBUG}/zathura/synctex/*.o)
ZATHURA_OBJECTS_GCOV  += $(wildcard ../${BUILDDIR_GCOV}/zathura/synctex/*.o)
LIBS   += $(ZLIB_LIB)

ifeq (,$(findstring -Isynctex,${CPPFLAGS}))
CPPFLAGS += -I../zathura/synctex
endif
ifeq (,$(findstring -DSYNCTEX_VERBOSE=0,${CPPFLAGS}))
CPPFLAGS += -DSYNCTEX_VERBOSE=0
endif
endif

ifneq ($(wildcard ${VALGRIND_SUPPRESSION_FILE}),)
VALGRIND_ARGUMENTS += --suppressions=${VALGRIND_SUPPRESSION_FILE}
endif

all: release

# release

${OBJECTS}: config.mk ../config.mk ../zathura/version.h

${BUILDDIR_RELEASE}/%.o: %.c
	$(call colorecho,CC,$<)
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT}: ${OBJECTS}
	$(QUIET)${MAKE} WITH_LIBFIU=1 -C .. ${BUILDDIR_RELEASE}/${BINDIR}/zathura
	$(call colorecho,CC,$@)
	@mkdir -p ${BUILDDIR_RELEASE}/${BINDIR}
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} \
		-o ${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT} \
		${OBJECTS} ${ZATHURA_OBJECTS} ${LIBS}

release: ${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT}

run: ${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT}
	$(QUIET)${FIU_EXEC} ./${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT}

# debug

${OBJECTS_DEBUG}: config.mk ../config.mk ../zathura/version.h

${BUILDDIR_DEBUG}/%.o: %.c
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(call colorecho,CC,$<)
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} ${DFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}: ${OBJECTS_DEBUG}
	$(QUIET)${MAKE} WITH_LIBFIU=1 -C .. ${BUILDDIR_DEBUG}/${BINDIR}/zathura
	$(call colorecho,CC,$@)
	@mkdir -p ${BUILDDIR_DEBUG}/${BINDIR}
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} \
		-o ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT} \
		${OBJECTS} ${ZATHURA_OBJECTS_DEBUG} ${LIBS}

debug: ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}

run-debug: ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}
	$(QUIET)${FIU_EXEC} ./${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}

# gcov

${OBJECTS_GCOV}: config.mk ../config.mk ../zathura/version.h

${BUILDDIR_GCOV}/%.o: %.c
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(call colorecho,CC,$<)
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} ${GCOV_CFLAGS} ${DFLAGS} ${GCOV_DFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

${BUILDDIR_GCOV}/${BINDIR}/${PROJECT}: ${OBJECTS_GCOV}
	$(QUIET)${MAKE} WITH_LIBFIU=1 -C .. ${BUILDDIR_GCOV}/${BINDIR}/zathura
	$(call colorecho,CC,$@)
	@mkdir -p ${BUILDDIR_GCOV}/${BINDIR}
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} ${GCOV_CFLAGS} ${GCOV_LDFLAGS} \
		-o ${BUILDDIR_GCOV}/${BINDIR}/${PROJECT} \
		${OBJECTS} ${ZATHURA_OBJECTS_GCOV} ${LIBS}

gcov: ${BUILDDIR_GCOV}/${BINDIR}/${PROJECT}

run-gcov: gcov
	$(QUIET)${FIU_EXEC} ./${BUILDDIR_GCOV}/${BINDIR}/${PROJECT}

../zathura/version.h:
	$(MAKE) -C .. zathura/version.h

valgrind: ${PROJECT}-debug
	 $(QUIET)G_SLICE=always-malloc G_DEBUG=gc-friendly ${FIU_EXEC} ${VALGRIND} ${VALGRIND_ARGUMENTS} ./${PROJECT}-debug

clean:
	$(call colorecho,RM, "Clean test files")
	$(QUIET)rm -rf ${PROJECT}
	$(QUIET)rm -rf ${PROJECT}-debug
	$(QUIET)rm -rf ${PROJECT}-gcov
	$(QUIET)rm -rf ${BUILDDIR}
	$(QUIET)rm -rf ${DEPENDDIR}

.PHONY: all options clean debug run

-include $(wildcard ${DEPENDDIR}/*.dep)
