# See LICENSE file for license and copyright information

include config.mk
include common.mk

PROJECT  = zathura
SOURCE   = $(shell find . -iname "*.c" -a ! -iname "database-*")
OBJECTS  = $(patsubst %.c, %.o,  $(SOURCE))
DOBJECTS = $(patsubst %.c, %.do, $(SOURCE))

ifeq (${DATABASE}, sqlite)
INCS   += $(SQLITE_INC)
LIBS   += $(SQLITE_LIB)
SOURCE += database-sqlite.c
else
ifeq (${DATABASE}, plain)
SOURCE += database-plain.c
endif
endif

all: options ${PROJECT}

options:
	@echo ${PROJECT} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LIBS    = ${LIBS}"
	@echo "DFLAGS  = ${DFLAGS}"
	@echo "CC      = ${CC}"

%.o: %.c
	$(ECHO) CC $<
	@mkdir -p .depend
	$(QUIET)${CC} -c ${CFLAGS} -o $@ $< -MMD -MF .depend/$@.dep

%.do: %.c
	$(ECHO) CC $<
	@mkdir -p .depend
	$(QUIET)${CC} -c ${CFLAGS} ${DFLAGS} -o $@ $< -MMD -MF .depend/$@.dep

# force recompilation of database.o if DATABASE has changed
database.o: database-${DATABASE}.o

${OBJECTS}:  config.mk
${DOBJECTS}: config.mk

${PROJECT}: ${OBJECTS}
	$(ECHO) CC -o $@
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} -o $@ ${OBJECTS} ${LIBS}

clean:
	$(QUIET)rm -rf ${PROJECT} ${OBJECTS} ${PROJECT}-${VERSION}.tar.gz \
		${DOBJECTS} ${PROJECT}-debug .depend ${PROJECT}.pc

${PROJECT}-debug: ${DOBJECTS}
	$(ECHO) CC -o $@
	$(QUIET)${CC} ${LDFLAGS} -o $@ ${DOBJECTS} ${LIBS}

debug: ${PROJECT}-debug

${PROJECT}.pc: ${PROJECT}.pc.in config.mk
	$(QUIET)echo project=${PROJECT} > ${PROJECT}.pc
	$(QUIET)echo version=${VERSION} >> ${PROJECT}.pc
	$(QUIET)echo includedir=${PREFIX}/include >> ${PROJECT}.pc
	$(QUIET)cat ${PROJECT}.pc.in >> ${PROJECT}.pc

valgrind: debug
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
		./${PROJECT}-debug

gdb: debug
	cgdb ${PROJECT}-debug

dist: clean
	$(QUIET)mkdir -p ${PROJECT}-${VERSION}
	$(QUIET)cp -R LICENSE Makefile config.mk README \
			${PROJECT}.1 ${SOURCE} ${PROJECT}.pc.in \
			${PROJECT}-${VERSION}
	$(QUIET)tar -cf ${PROJECT}-${VERSION}.tar ${PROJECT}-${VERSION}
	$(QUIET)gzip ${PROJECT}-${VERSION}.tar
	$(QUIET)rm -rf ${PROJECT}-${VERSION}

install: all ${PROJECT}.pc
	$(ECHO) installing executable file
	$(QUIET)mkdir -p ${DESTDIR}${PREFIX}/bin
	$(QUIET)install -m 755 ${PROJECT} ${DESTDIR}${PREFIX}/bin
	$(ECHO) installing header files
	$(QUIET)mkdir -p ${DESTDIR}${PREFIX}/include/${PROJECT}
	$(QUIET)cp -f document.h ${DESTDIR}${PREFIX}/include/${PROJECT}
	$(QUIET)cp -f zathura.h ${DESTDIR}${PREFIX}/include/${PROJECT}
	$(ECHO) installing manual pages
	$(QUIET)mkdir -p ${DESTDIR}${MANPREFIX}/man1
	$(QUIET)sed "s/VERSION/${VERSION}/g" < ${PROJECT}.1 > ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	$(QUIET)if which rst2man > /dev/null ; then \
		mkdir -p ${DESTDIR}${MANPREFIX}/man5 ; \
		rst2man ${PROJECT}rc.5.rst > ${DESTDIR}${MANPREFIX}/man5/${PROJECT}rc.5 ; \
	fi
	$(QUIET)mkdir -p ${DESTDIR}${DESKTOPPREFIX}
	$(ECHO) installing desktop file
	$(QUIET)install -m 644 ${PROJECT}.desktop ${DESTDIR}${DESKTOPPREFIX}
	$(QUIET)chmod 644 ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	$(ECHO) installing pkgconfig file
	$(QUIET)mkdir -p ${DESTDIR}${PREFIX}/lib/pkgconfig
	$(QUIET)cp -f ${PROJECT}.pc ${DESTDIR}${PREFIX}/lib/pkgconfig

uninstall:
	$(ECHO) removing executable file
	$(QUIET)rm -f ${DESTDIR}${PREFIX}/bin/${PROJECT}
	$(ECHO) removing header files
	$(QUIET)rm -rf ${DESTDIR}${PREFIX}/include/${PROJECT}
	$(ECHO) removing manual pages
	$(QUIET)rm -f ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	$(QUIET)rm -f ${DESTDIR}${MANPREFIX}/man5/${PROJECT}rc.5
	$(ECHO) removing desktop file
	$(QUIET)rm -f ${DESTDIR}${DESKTOPPREFIX}/${PROJECT}.desktop
	$(ECHO) removing pkgconfig file
	$(QUIET)rm -f ${DESTDIR}${PREFIX}/lib/pkgconfig/${PROJECT}.pc

-include $(wildcard .depend/*.dep)

.PHONY: all options clean debug valgrind gdb dist install uninstall
