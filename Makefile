# See LICENSE file for license and copyright information

include config.mk

PROJECT  = zathura
SOURCE   = $(shell find . -iname "*.c")
OBJECTS  = $(patsubst %.c, %.o,  $(SOURCE))
DOBJECTS = $(patsubst %.c, %.do, $(SOURCE))

all: options ${PROJECT}

options:
	@echo ${PROJECT} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LIBS    = ${LIBS}"
	@echo "DFLAGS  = ${DFLAGS}"
	@echo "CC      = ${CC}"

%.o: %.c
	@echo CC $<
	@mkdir -p .depend
	@${CC} -c ${CFLAGS} -o $@ $< -MMD -MF .depend/$@.dep

%.do: %.c
	@echo CC $<
	@mkdir -p .depend
	@${CC} -c ${CFLAGS} ${DFLAGS} -o $@ $< -MMD -MF .depend/$@.dep

${OBJECTS}:  config.mk
${DOBJECTS}: config.mk

${PROJECT}: ${OBJECTS}
	@echo CC -o $@
	@${CC} ${SFLAGS} ${LDFLAGS} -o $@ ${OBJECTS} ${LIBS}

clean:
	@rm -rf ${PROJECT} ${OBJECTS} ${PROJECT}-${VERSION}.tar.gz \
		${DOBJECTS} ${PROJECT}-debug .depend ${PROJECT}.pc

${PROJECT}-debug: ${DOBJECTS}
	@echo CC -o ${PROJECT}-debug
	@${CC} ${LDFLAGS} -o ${PROJECT}-debug ${DOBJECTS} ${LIBS}

debug: ${PROJECT}-debug

${PROJECT}.pc: ${PROJECT}.pc.in config.mk
	@echo project=${PROJECT} > ${PROJECT}.pc
	@echo version=${VERSION} >> ${PROJECT}.pc
	@echo includedir=${PREFIX}/include >> ${PROJECT}.pc
	@cat ${PROJECTNV}.pc.in >> ${PROJECT}.pc

valgrind: debug
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes \
		./${PROJECT}-debug

gdb: debug
	cgdb ${PROJECT}-debug

dist: clean
	@${MAKE} -p ${PROJECT}-${VERSION}
	@cp -R LICENSE Makefile config.mk README \
			${PROJECT}.1 ${SOURCE} ${PROJECT}-${VERSION} \
			${PROJECT}.pc.in
	@tar -cf ${PROJECT}-${VERSION}.tar ${PROJECT}-${VERSION}
	@gzip ${PROJECT}-${VERSION}.tar
	@rm -rf ${PROJECT}-${VERSION}

install: all ${PROJECT}.pc
	@echo installing executable file
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f ${PROJECT} ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${PROJECT} ${DESTDIR}${PREFIX}/bin/${PROJECT}
	@echo installing header file
	@mkdir -p ${DESTDIR}${PREFIX}/include/${PROJECT}
	@cp -f document.h ${DESTDIR}${PREFIX}/include/${PROJECT}
	@cp -f zathura.h ${DESTDIR}${PREFIX}/include/${PROJECT}
	@echo installing manual page
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < ${PROJECT}.1 > ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	@echo installing pkgconfig file
	@mkdir -p ${DESTDIR}${PREFIX}/lib/pkgconfig
	@cp -f ${PROJECT}.pc ${DESTDIR}${PREFIX}/lib/pkgconfig

uninstall:
	@echo removing executable file
	@rm -f ${DESTDIR}${PREFIX}/bin/${PROJECT}
	@echo removing header file
	@rm -f ${DESTDIR}${PREFIX}/include/${PROJECT}/document.h
	@rm -f ${DESTDIR}${PREFIX}/include/${PROJECT}/zathura.h
	@echo removing manual page
	@rm -f ${DESTDIR}${MANPREFIX}/man1/${PROJECT}.1
	@echo removing pkgconfig file
	@rm -f ${DESTDIR}${PREFIX}/lib/pkgconfig

-include $(wildcard .depend/*.dep)

.PHONY: all options clean debug valgrind gdb dist install uninstall
