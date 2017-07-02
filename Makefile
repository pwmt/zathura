# See LICENSE file for license and copyright information

include config.mk
include colors.mk
include common.mk

# source files
OSOURCE       = $(sort $(wildcard ${PROJECT}/*.c) \
                ${PROJECT}/css-definitions.c)
SOURCE_FILTER =

ifneq (${WITH_SQLITE},0)
CPPFLAGS += -DWITH_SQLITE
else
SOURCE_FILTER += ${PROJECT}/database-sqlite.c
endif

ifneq ($(WITH_MAGIC),0)
CPPFLAGS += -DWITH_MAGIC
endif

ifneq ($(WITH_SYNCTEX),0)
CPPFLAGS += -DWITH_SYNCTEX
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
ifeq (,$(findstring -DDBUSINTERFACEDIR,${CPPFLAGS}))
ifneq ($(WITH_LOCAL_DBUS_XML),0)
CPPFLAGS += -DDBUSINTERFACEDIR=\"$(abspath data)\"
else
CPPFLAGS += -DDBUSINTERFACEDIR=\"${DBUSINTERFACEDIR}\"
endif
endif

SOURCE        = $(filter-out $(SOURCE_FILTER),$(OSOURCE))
OBJECTS       = $(addprefix ${BUILDDIR_RELEASE}/,${SOURCE:.c=.o})
OBJECTS_DEBUG = $(addprefix ${BUILDDIR_DEBUG}/,${SOURCE:.c=.o})
OBJECTS_GCOV  = $(addprefix ${BUILDDIR_GCOV}/,${SOURCE:.c=.o})
HEADERINST    = $(addprefix ${PROJECT}/,version.h document.h macros.h page.h types.h plugin-api.h links.h)

all: options ${PROJECT} po build-manpages

# pkg-config based version checks
.version-checks/%: config.mk
	$(QUIET)test $($(*)_VERSION_CHECK) -eq 0 || \
		${PKG_CONFIG} --atleast-version $($(*)_MIN_VERSION) $($(*)_PKG_CONFIG_NAME) || ( \
		echo "The minimum required version of $(*) is $($(*)_MIN_VERSION)" && \
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

# generated files

${PROJECT}/version.h: ${PROJECT}/version.h.in config.mk
	$(QUIET)sed -e 's/ZVMAJOR/${ZATHURA_VERSION_MAJOR}/' \
		-e 's/ZVMINOR/${ZATHURA_VERSION_MINOR}/' \
		-e 's/ZVREV/${ZATHURA_VERSION_REV}/' \
		-e 's/ZVAPI/${ZATHURA_API_VERSION}/' \
		-e 's/ZVABI/${ZATHURA_ABI_VERSION}/' ${PROJECT}/version.h.in > ${PROJECT}/version.h.tmp
	$(QUIET)mv ${PROJECT}/version.h.tmp ${PROJECT}/version.h

${PROJECT}/css-definitions.%: data/zathura-css.gresource.xml config.mk
	$(call colorecho,GEN,$@)
	@mkdir -p ${DEPENDDIR}/$(dir $@)
	$(QUIET)$(GLIB_COMPILE_RESOURCES) --generate --c-name=zathura_css --internal \
		--dependency-file=$(DEPENDDIR)/$@.dep \
		--sourcedir=data --target=$@ data/zathura-css.gresource.xml

# common dependencies

${OBJECTS} ${OBJECTS_DEBUG} ${OBJECTS_GCOV}: config.mk \
	${PROJECT}/version.h ${PROJECT}/css-definitions.h \
	.version-checks/GIRARA .version-checks/GLIB .version-checks/GTK

# rlease build

${BUILDDIR_RELEASE}/%.o: %.c
	$(call colorecho,CC,$<)
	@mkdir -p ${DEPENDDIR}/$(dir $@)
	@mkdir -p $(dir $(abspath $@))
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} -o $@ $< -MMD -MF ${DEPENDDIR}/$@.dep

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

${BUILDDIR_DEBUG}/%.o: %.c
	$(call colorecho,CC,$<)
	@mkdir -p ${DEPENDDIR}/$(dir $@)
	@mkdir -p $(dir $(abspath $@))
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} ${DFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$@.dep

${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}: ${OBJECTS_DEBUG}
	$(call colorecho,CC,$@)
	@mkdir -p ${BUILDDIR_DEBUG}/${BINDIR}
	$(QUIET)${CC} ${LDFLAGS} \
		-o ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT} ${OBJECTS_DEBUG} ${LIBS}

debug: ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}

run-debug: debug
	$(QUIET)./${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}

# gcov build

${BUILDDIR_GCOV}/%.o: %.c
	$(call colorecho,CC,$<)
	@mkdir -p ${DEPENDDIR}/$(dir $@)
	@mkdir -p $(dir $(abspath $@))
	$(QUIET)${CC} -c ${CPPFLAGS} ${CFLAGS} ${GCOV_CFLAGS} \
		-o $@ $< -MMD -MF ${DEPENDDIR}/$@.dep

${BUILDDIR_GCOV}/${BINDIR}/${PROJECT}: ${OBJECTS_GCOV}
	$(call colorecho,CC,$@)
	@mkdir -p ${BUILDDIR_GCOV}/${BINDIR}
	$(QUIET)${CC} ${LDFLAGS} ${GCOV_CFLAGS} ${GCOV_LDFLAGS} \
		-o ${BUILDDIR_GCOV}/${BINDIR}/${PROJECT} ${OBJECTS_GCOV} ${LIBS}

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
		 ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}

gdb: debug
	$(QUIET)cgdb ${BUILDDIR_DEBUG}/${BINDIR}/${PROJECT}

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

DEPENDS = ${DEPENDDIRS:^=${DEPENDDIR}/}$(addprefix ${DEPENDDIR}/,${OBJECTS:.o=.o.dep})
-include ${DEPENDS}

.PHONY: all options clean doc debug valgrind gdb dist doc install uninstall \
	test po install-headers uninstall-headers update-po install-manpages \
	build-manpages install-dbus
