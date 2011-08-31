# See LICENSE file for license and copyright information
# zathura - user interface

include config.mk
include common.mk

PROJECT  = zathura
SOURCE   = zathura.c
OBJECTS  = ${SOURCE:.c=.o}
DOBJECTS = ${SOURCE:.c=.do}

all: options ${PROJECT}

options:
	@echo ${PROJECT} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "LIBS    = ${LIBS}"
	@echo "DFLAGS  = ${DFLAGS}"
	@echo "CC      = ${CC}"

%.o: %.c
	$(ECHO) CC $<
	$(QUIET)${CC} -c ${CFLAGS} -o $@ $<

%.do: %.c
	$(ECHO) CC $<
	$(QUIET)${CC} -c ${CFLAGS} ${DFLAGS} -o $@ $<

${OBJECTS}:  config.h config.mk
${DOBJECTS}: config.h config.mk

config.h: config.def.h
	@if [ -f $@ ] ; then \
		echo "config.h exists, but config.def.h is newer. Please check your" \
		"config.h or ${PROJECT} might fail to build." ; \
	else \
		cp $< $@ ; \
	fi

${PROJECT}: ${OBJECTS}
	$(ECHO) CC -o $@
	$(QUIET)${CC} ${SFLAGS} ${LDFLAGS} -o $@ ${OBJECTS} ${LIBS}

clean:
	$(QUIET)rm -rf ${PROJECT} ${OBJECTS} ${PROJECT}-${VERSION}.tar.gz \
		${DOBJECTS} ${PROJECT}-debug

distclean: clean
	$(QUIET)rm -rf config.h

${PROJECT}-debug: ${DOBJECTS}
	$(ECHO) CC -o ${PROJECT}-debug
	$(QUIET)${CC} ${LDFLAGS} -o ${PROJECT}-debug ${DOBJECTS} ${LIBS}

debug: ${PROJECT}-debug

valgrind: debug
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
		./${PROJECT}-debug

gdb: debug
	cgdb ${PROJECT}-debug

dist: clean
	$(QUIET)mkdir -p ${PROJECT}-${VERSION}
	$(QUIET)cp -R LICENSE Makefile config.mk config.def.h README \
			${PROJECT}.desktop ${PROJECT}rc.5.rst \
			${PROJECT}.1 ${SOURCE} ${PROJECT}-${VERSION}
	$(QUIET)tar -cf ${PROJECT}-${VERSION}.tar ${PROJECT}-${VERSION}
	$(QUIET)gzip ${PROJECT}-${VERSION}.tar
	$(QUIET)rm -rf ${PROJECT}-${VERSION}

install: all
	$(ECHO) installing executable file
	$(QUIET)mkdir -p ${DESTDIR}${PREFIX}/bin
	$(QUIET)install -m 755 ${PROJECT} ${DESTDIR}${PREFIX}/bin
	$(ECHO) installing manual page
	$(QUIET)mkdir -p ${DESTDIR}${MANPREFIX}/man1
	$(QUIET)sed "s/VERSION/${VERSION}/g" < ${PROJECT}.1 > ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	$(QUIET)if which rst2man > /dev/null ; then \
		mkdir -p ${DESTDIR}${MANPREFIX}/man5 ; \
		rst2man ${PROJECT}rc.5.rst > ${DESTDIR}${MANPREFIX}/man5/${PROJECT}rc.5 ; \
	fi
	$(QUIET)chmod 644 ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	$(QUIET)mkdir -p ${DESTDIR}${DESKTOPPREFIX}
	$(ECHO) installing desktop file
	$(QUIET)install -m 644 ${PROJECT}.desktop ${DESTDIR}${DESKTOPPREFIX}

uninstall:
	$(ECHO) removing executable file
	$(QUIET)rm -f ${DESTDIR}${PREFIX}/bin/${PROJECT}
	$(ECHO) removing manual page
	$(QUIET)rm -f ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	$(QUIET)rm -f ${DESTDIR}${MANPREFIX}/man5/${PROJECT}rc.5
	$(ECHO) removing desktop file
	$(QUIET)rm -f ${DESTDIR}${DESKTOPPREFIX}/${PROJECT}.desktop
