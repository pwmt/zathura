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
ZATHURA_OBJECTS += $(wildcard ../${BUILDDIR_RELEASE}/zathura/synctex/*.o)
LIBS   += $(ZLIB_LIB)

ifeq (,$(findstring -Isynctex,${CPPFLAGS}))
CPPFLAGS += -I${PROJECT}/synctex
endif
ifeq (,$(findstring -DSYNCTEX_VERBOSE=0,${CPPFLAGS}))
CPPFLAGS += -DSYNCTEX_VERBOSE=0
endif
endif

ifneq ($(wildcard ${VALGRIND_SUPPRESSION_FILE}),)
VALGRIND_ARGUMENTS += --suppressions=${VALGRIND_SUPPRESSION_FILE}
endif

ifeq (,$(findstring -DZATHURA_PLUGINDIR,${CPPFLAGS}))
CPPFLAGS += -DZATHURA_PLUGINDIR=\"${PLUGINDIR}\"
endif
ifeq (,$(findstring -DGETTEXT_PACKAGE,${CPPFLAGS}))
CPPFLAGS += -DGETTEXT_PACKAGE=\"${PROJECT}\"
endif
ifeq (,$(findstring -DLOCALEDIR,${CPPFLAGS}))
CPPFLAGS += -DLOCALEDIR=\"${LOCALEDIR}\"
endif

all: ${PROJECT}

# release

${PROJECT}: options ${OBJECTS}
	@echo "zathura objects: ${ZATHURA_OBJECTS}"
	$(QUIET)${MAKE} WITH_LIBFIU=1 -C .. zathura
	$(call colorecho,CC,$@)
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} -o $@ \
		${OBJECTS} ${ZATHURA_OBJECTS} ${LIBS} ${ZATHURA_RELEASE}

${OBJECTS}: config.mk ../config.mk ../zathura/version.h

${BUILDDIR_RELEASE}/%.o: %.c
	$(call colorecho,CC,$<)
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

run: ${PROJECT}
	$(QUIET)${FIU_EXEC} ./${PROJECT}

# debug

debug: options ${PROJECT}-debug

${PROJECT}-debug: ${OBJECTS_DEBUG}
	$(QUIET)${MAKE} WITH_LIBFIU=1 -C .. zathura-debug
	$(call colorecho,CC,$@)
	$(QUIET)${CC} ${LDFLAGS} -o $@ \
		${OBJECTS_DEBUG} ${ZATHURA_OBJECTS_DEBUG} ${LIBS} ${ZATHURA_DEBUG}

${OBJECTS_DEBUG}: config.mk ../config.mk ../zathura/version.h

${BUILDDIR_DEBUG}/%.o: %.c
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(call colorecho,CC,$<)
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} ${DFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

run-debug: ${PROJECT}-debug
	$(QUIET)${FIU_EXEC} ./${PROJECT}-debug

# gcov

gcov: options ${PROJECT}-gcov

${PROJECT}-gcov: options ${OBJECTS_GCOV}
	$(QUIET)${MAKE} WITH_LIBFIU=1 -C .. zathura-gcov
	$(call colorecho,CC,$@)
	$(QUIET)${CC} ${LDFLAGS} ${GCOV_LDFLAGS} -o $@ \
		${OBJECTS_GCOV} ${ZATHURA_OBJECTS_GCOV} ${LIBS} ${ZATHURA_GCOV}

${OBJECTS_GCOV}: config.mk ../config.mk ../zathura/version.h

${BUILDDIR_GCOV}/%.o: %.c
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(call colorecho,CC,$<)
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} ${GCOV_CFLAGS} ${DFLAGS} ${GCOV_DFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

run-gcov: ${PROJECT}-gcov
	$(QUIET)${FIU_EXEC} ./${PROJECT}-gcov

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

	$(QUIET)${MAKE} -C plugin clean

.PHONY: all options clean debug run

-include $(wildcard ${DEPENDDIR}/*.dep)
