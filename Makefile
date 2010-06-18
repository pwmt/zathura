# See LICENSE file for license and copyright information
# zathura - user interface

include config.mk

PROJECT  = zathura
SOURCE   = zathura.c
OBJECTS  = ${SOURCE:.c=.o}
DOBJECTS = ${SOURCE:.c=.do}

all: options ${PROJECT}

options:
	@echo ${PROJECT} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LIBS    = ${LIBS}"
	@echo "DFLAGS  = ${DFLAGS}"
	@echo "CC      = ${CC}"

%.o: %.c
	@echo CC $<
	@${CC} -c ${CFLAGS} -o $@ $<

%.do: %.c
	@echo CC $<
	@${CC} -c ${CFLAGS} ${DFLAGS} -o $@ $<

${OBJECTS}:  config.h config.mk
${DOBJECTS}: config.h config.mk

config.h:
	@cp config.def.h $@

${PROJECT}: ${OBJECTS}
	@echo CC -o $@
	@${CC} -s ${LDFLAGS} -o $@ ${OBJECTS} ${LIBS}

clean:
	@rm -rf ${PROJECT} ${OBJECTS} ${PROJECT}-${VERSION}.tar.gz \
		${DOBJECTS} ${PROJECT}-debug

${PROJECT}-debug: ${DOBJECTS}
	@echo CC -o ${PROJECT}-debug
	@${CC} ${LDFLAGS} -o ${PROJECT}-debug ${DOBJECTS} ${LIBS}

debug: ${PROJECT}-debug

valgrind: debug
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
		./${PROJECT}-debug

gdb: debug
	cgdb ${PROJECT}-debug

dist: clean
	@mkdir -p ${PROJECT}-${VERSION}
	@cp -R LICENSE Makefile config.mk config.def.h README \
			${PROJECT}.1 ${SOURCE} ${PROJECT}-${VERSION}
	@tar -cf ${PROJECT}-${VERSION}.tar ${PROJECT}-${VERSION}
	@gzip ${PROJECT}-${VERSION}.tar
	@rm -rf ${PROJECT}-${VERSION}

install: all
	@echo installing executeable file
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f ${PROJECT} ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${PROJECT} ${DESTDIR}${PREFIX}/bin/${PROJECT}
	@echo installing manual page
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < ${PROJECT}.1 > ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1

uninstall:
	@echo removing executeable file
	@rm -f ${DESTDIR}${MANPREFIX}/bin/${PROJECT}
	@echo removing manual page
	@rm -f ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
