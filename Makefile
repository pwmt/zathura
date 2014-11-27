# See LICENSE file for license and copyright information

include config.mk
include colors.mk
include common.mk

OSOURCE    = $(filter-out ${PROJECT}/css-definitions.c, $(filter-out ${PROJECT}/dbus-interface-definitions.c, $(wildcard ${PROJECT}/*.c)))

ifneq (${WITH_SQLITE},0)
INCS     += $(SQLITE_INC)
LIBS     += $(SQLITE_LIB)
SOURCE    = $(OSOURCE)
CPPFLAGS += -DWITH_SQLITE
else
SOURCE    = $(filter-out ${PROJECT}/database-sqlite.c,$(OSOURCE))
endif

ifneq ($(WITH_MAGIC),0)
INCS     += $(MAGIC_INC)
LIBS     += $(MAGIC_LIB)
CPPFLAGS += -DWITH_MAGIC
endif

ifneq ($(WITH_SYSTEM_SYNCTEX),0)
INCS   += $(SYNCTEX_INC)
LIBS   += $(SYNCTEX_LIB)
else
INCS   += $(ZLIB_INC)
LIBS   += $(ZLIB_LIB)
SOURCE += $(wildcard ${PROJECT}/synctex/*.c)

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

OBJECTS       = $(addprefix ${BUILDDIR_RELEASE}/,${SOURCE:.c=.o}) \
	${BUILDDIR_RELEASE}/${PROJECT}/css-definitions.o \
	${BUILDDIR_RELEASE}/${PROJECT}/dbus-interface-definitions.o
OBJECTS_DEBUG = $(addprefix ${BUILDDIR_DEBUG}/,${SOURCE:.c=.o}) \
	${BUILDDIR_DEBUG}/${PROJECT}/css-definitions.o \
	${BUILDDIR_DEBUG}/${PROJECT}/dbus-interface-definitions.o
OBJECTS_GCOV  = $(addprefix ${BUILDDIR_GCOV}/,${SOURCE:.c=.o}) \
	${BUILDDIR_GCOV}/${PROJECT}/css-definitions.o \
	${BUILDDIR_GCOV}/${PROJECT}/dbus-interface-definitions.o
HEADER        = $(wildcard ${PROJECT}/*.h) $(wildcard synctex/*.h)
HEADERINST    = $(addprefix ${PROJECT}/,version.h document.h macros.h page.h types.h plugin-api.h links.h)

all: options ${PROJECT} po build-manpages

# pkg-config based version checks
.version-checks/%: config.mk
	$(QUIET)test $($(*)_VERSION_CHECK) -eq 0 || \
		pkg-config --atleast-version $($(*)_MIN_VERSION) $($(*)_PKG_CONFIG_NAME) || ( \
		echo "The minium required version of $(*) is $($(*)_MIN_VERSION)" && \
		false \
	)
	@mkdir -p .version-checks
	$(QUIET)touch $@

options:
	@echo ${PROJECT} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LIBS    = ${LIBS}"
	@echo "DFLAGS  = ${DFLAGS}"
	@echo "CC      = ${CC}"

${PROJECT}/version.h: ${PROJECT}/version.h.in config.mk
	$(QUIET)sed -e 's/ZVMAJOR/${ZATHURA_VERSION_MAJOR}/' \
		-e 's/ZVMINOR/${ZATHURA_VERSION_MINOR}/' \
		-e 's/ZVREV/${ZATHURA_VERSION_REV}/' \
		-e 's/ZVAPI/${ZATHURA_API_VERSION}/' \
		-e 's/ZVABI/${ZATHURA_ABI_VERSION}/' ${PROJECT}/version.h.in > ${PROJECT}/version.h.tmp
	$(QUIET)mv ${PROJECT}/version.h.tmp ${PROJECT}/version.h

${PROJECT}/dbus-interface-definitions.c: data/org.pwmt.zathura.xml
	$(QUIET)echo '#include "dbus-interface-definitions.h"' > $@.tmp
	$(QUIET)echo 'const char* DBUS_INTERFACE_XML =' >> $@.tmp
	$(QUIET)sed 's/^\(.*\)$$/"\1\\n"/' data/org.pwmt.zathura.xml >> $@.tmp
	$(QUIET)echo ';' >> $@.tmp
	$(QUIET)mv $@.tmp $@

${PROJECT}/css-definitions.c: data/zathura.css_t
	$(QUIET)echo '#include "css-definitions.h"' > $@.tmp
	$(QUIET)echo 'const char* CSS_TEMPLATE_INDEX =' >> $@.tmp
	$(QUIET)sed 's/^\(.*\)$$/"\1\\n"/' $< >> $@.tmp
	$(QUIET)echo ';' >> $@.tmp
	$(QUIET)mv $@.tmp $@

# release build

${OBJECTS}: config.mk ${PROJECT}/version.h \
	.version-checks/GIRARA .version-checks/GLIB .version-checks/GTK

${BUILDDIR_RELEASE}/%.o: %.c
	$(call colorecho,CC,$<)
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} -o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT}: ${OBJECTS}
	$(call colorecho,CC,$@)
	@mkdir -p ${BUILDDIR_RELEASE}/${BINDIR}
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} \
		-o ${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT} ${OBJECTS} ${LIBS}

${PROJECT}: ${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT}

release: ${PROJECT}

run: release
	$(QUIET)./${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT}

# debug build

${OBJECTS_DEBUG}: config.mk ${PROJECT}/version.h \
	.version-checks/GIRARA .version-checks/GLIB .version-checks/GTK

${BUILDDIR_DEBUG}/%.o: %.c
	$(call colorecho,CC,$<)
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} ${DFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}: ${OBJECTS_DEBUG}
	$(call colorecho,CC,$@)
	@mkdir -p ${BUILDDIR_DEBUG}/${BINDIR}
	$(QUIET)${CC} ${LDFLAGS} \
		-o ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT} ${OBJECTS_DEBUG} ${LIBS}

debug: ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}

run-debug: debug
	$(QUIET)./${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}

# gcov build

${OBJECTS_GCOV}: config.mk ${PROJECT}/version.h \
	.version-checks/GIRARA .version-checks/GLIB .version-checks/GTK

${BUILDDIR_GCOV}/%.o: %.c
	$(call colorecho,CC,$<)
	@mkdir -p ${DEPENDDIR}/$(dir $(abspath $@))
	@mkdir -p $(dir $(abspath $@))
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} ${GCOV_CFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$(abspath $@).dep

${BUILDDIR_GCOV}/${BINDIR}/${PROJECT}: ${OBJECTS_GCOV}
	$(call colorecho,CC,$@)
	@mkdir -p ${BUILDDIR_GCOV}/${BINDIR}
	$(QUIET)${CC} ${LDFLAGS} ${GCOV_CFLAGS} ${GCOV_LDFLAGS} \
		-o ${BUILDDIR_GCOV}/${BINDIR}/${PROJECT} ${OBJECTS_GCOv} ${LIBS}

gcov: options ${BUILDDIR_GCOV}/${BINDIR}/${PROJECT}
	$(QUIET)${MAKE} -C tests run-gcov
	$(call colorecho,LCOV,"Analyse data")
	$(QUIET)${LCOV_EXEC} ${LCOV_FLAGS}
	$(call colorecho,LCOV,"Generate report")
	$(QUIET)${GENHTML_EXEC} ${GENHTML_FLAGS}

run-gcov: ${BUILDDIR_GCOV}/${BINDIR}/${PROJECT}
	$(QUIET)./${BUILDDIR_GCOV}/${BINDIR}/${PROJECT}

# clean

clean:
	$(QUIET)rm -rf \
		${BUILDDIR} \
		${DEPENDDIR} \
		${TARFILE} \
		${TARDIR} \
		${PROJECT}.pc \
		${PROJECT}/version.h \
		${PROJECT}/version.h.tmp \
		${PROJECT}/dbus-interface-definitions.c \
		${PROJECT}/dbus-interface-definitions.c.tmp \
		${PROJECT}/css-definitions.c \
		${PROJECT}/css-definitions.c.tmp \
		$(PROJECT).info \
		gcov \
		.version-checks
	$(QUIET)$(MAKE) -C tests clean
	$(QUIET)$(MAKE) -C po clean
	$(QUIET)$(MAKE) -C doc clean

${PROJECT}.pc: ${PROJECT}.pc.in config.mk
	$(QUIET)echo project=${PROJECT} > ${PROJECT}.pc
	$(QUIET)echo version=${VERSION} >> ${PROJECT}.pc
	$(QUIET)echo apiversion=${ZATHURA_API_VERSION} >> ${PROJECT}.pc
	$(QUIET)echo abiversion=${ZATHURA_ABI_VERSION} >> ${PROJECT}.pc
	$(QUIET)echo includedir=${INCLUDEDIR} >> ${PROJECT}.pc
	$(QUIET)echo plugindir=${PLUGINDIR} >> ${PROJECT}.pc
	$(QUIET)echo GTK_VERSION=3 >> ${PROJECT}.pc
	$(QUIET)cat ${PROJECT}.pc.in >> ${PROJECT}.pc

valgrind: debug
	 $(QUIET)G_SLICE=always-malloc G_DEBUG=gc-friendly ${VALGRIND} ${VALGRIND_ARGUMENTS} \
		 ./${PROJECT}-debug

gdb: debug
	$(QUIET)cgdb ${PROJECT}-debug

test: ${OBJECTS}
	$(QUIET)$(MAKE) -C tests run

dist: clean build-manpages
	$(QUIET)tar -czf $(TARFILE) --exclude=.gitignore \
		--transform 's,^,zathura-$(VERSION)/,' \
		`git ls-files` \
		doc/_build/$(PROJECT).1 doc/_build/$(PROJECT)rc.5

doc:
	$(QUIET)$(MAKE) -C doc

po:
	$(QUIET)${MAKE} -C po

update-po:
	$(QUIET)${MAKE} -C po update-po

build-manpages:
	$(QUIET)${MAKE} -C doc man

install-manpages: build-manpages
	$(call colorecho,INSTALL,"man pages")
	$(QUIET)mkdir -m 755 -p ${DESTDIR}${MANPREFIX}/man1 ${DESTDIR}${MANPREFIX}/man5
ifneq "$(wildcard doc/_build/${PROJECT}.1)" ""
	$(QUIET)install -m 644 doc/_build/${PROJECT}.1 ${DESTDIR}${MANPREFIX}/man1
endif
ifneq "$(wildcard doc/_build/${PROJECT}rc.5)" ""
	$(QUIET)install -m 644 doc/_build/${PROJECT}rc.5 ${DESTDIR}${MANPREFIX}/man5
endif

install-headers: ${PROJECT}.pc
	$(call colorecho,INSTALL,"header files")
	$(QUIET)mkdir -m 755 -p ${DESTDIR}${INCLUDEDIR}/${PROJECT}
	$(QUIET)install -m 644 ${HEADERINST} ${DESTDIR}${INCLUDEDIR}/${PROJECT}

	$(call colorecho,INSTALL,"pkgconfig file")
	$(QUIET)mkdir -m 755 -p ${DESTDIR}${LIBDIR}/pkgconfig
	$(QUIET)install -m 644 ${PROJECT}.pc ${DESTDIR}${LIBDIR}/pkgconfig

install-dbus:
	$(call colorecho,INSTALL,"D-Bus interface definitions")
	$(QUIET)mkdir -m 755 -p $(DESTDIR)$(DBUSINTERFACEDIR)
	$(QUIET)install -m 644 data/org.pwmt.zathura.xml $(DESTDIR)$(DBUSINTERFACEDIR)

install-appdata:
	$(call colorecho,INSTALL,"AppData file")
	$(QUIET)mkdir -m 755 -p $(DESTDIR)$(APPDATAPREFIX)
	$(QUIET)install -m 644 data/$(PROJECT).appdata.xml $(DESTDIR)$(APPDATAPREFIX)

install: all install-headers install-manpages install-dbus install-appdata
	$(call colorecho,INSTALL,"executeable file")
	$(QUIET)mkdir -m 755 -p ${DESTDIR}${PREFIX}/bin
	$(QUIET)install -m 755 ${BUILDDIR_RELEASE}/${BINDIR}/${PROJECT} ${DESTDIR}${PREFIX}/bin
	$(QUIET)mkdir -m 755 -p ${DESTDIR}${DESKTOPPREFIX}
	$(call colorecho,INSTALL,"desktop file")
	$(QUIET)install -m 644 ${PROJECT}.desktop ${DESTDIR}${DESKTOPPREFIX}
	$(MAKE) -C po install

uninstall-headers:
	$(call colorecho,UNINSTALL,"header files")
	$(QUIET)rm -rf ${DESTDIR}${INCLUDEDIR}/${PROJECT}
	$(call colorecho,UNINSTALL,"pkgconfig file")
	$(QUIET)rm -f ${DESTDIR}${LIBDIR}/pkgconfig/${PROJECT}.pc

uninstall: uninstall-headers
	$(ECHO) removing executable file
	$(call colorecho,UNINSTALL,"executeable")
	$(QUIET)rm -f ${DESTDIR}${PREFIX}/bin/${PROJECT}
	$(call colorecho,UNINSTALL,"man pages")
	$(QUIET)rm -f ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	$(QUIET)rm -f ${DESTDIR}${MANPREFIX}/man5/${PROJECT}rc.5
	$(call colorecho,UNINSTALL,"desktop file")
	$(QUIET)rm -f ${DESTDIR}${DESKTOPPREFIX}/${PROJECT}.desktop
	$(call colorecho,UNINSTALL,"D-Bus interface definitions")
	$(QUIET)rm -f $(DESTDIR)$(DBUSINTERFACEDIR)/org.pwmt.zathura.xml
	$(call colorecho,UNINSTALL,"AppData file")
	$(QUIET)rm -f $(DESTDIR)$(APPDATAPREFIX)/$(PROJECT).appdata.xml
	$(MAKE) -C po uninstall

-include $(wildcard .depend/*.dep)

.PHONY: all options clean doc debug valgrind gdb dist doc install uninstall \
	test po install-headers uninstall-headers update-po install-manpages \
	build-manpages install-dbus
