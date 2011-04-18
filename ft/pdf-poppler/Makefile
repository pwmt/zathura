# See LICENSE file for license and copyright information

include config.mk

PLUGIN   = pdf
SOURCE   = pdf.c
OBJECTS  = ${SOURCE:.c=.o}
DOBJECTS = ${SOURCE:.c=.do}

all: options ${PLUGIN}

options:
	@echo ${PLUGIN} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "DFLAGS  = ${DFLAGS}"
	@echo "CC      = ${CC}"

%.o: %.c
	@echo CC $<
	@${CC} -c ${CFLAGS} -o $@ $<

%.do: %.c
	@echo CC $<
	@${CC} -c ${CFLAGS} ${DFLAGS} -o $@ $<

${OBJECTS}:  config.mk
${DOBJECTS}: config.mk

${PLUGIN}: ${OBJECTS}
	@echo LD $@
	@${CC} -shared ${LDFLAGS} -o ${PLUGIN}.so $(OBJECTS) ${LIBS}

${PLUGIN}-debug: ${DOBJECTS}
	@echo LD $@
	@${CC} -shared ${LDFLAGS} -o ${PLUGIN}.so $(DOBJECTS) ${LIBS}

clean:
	@rm -rf ${OBJECTS} ${DOBJECTS} $(PLUGIN).so

debug: options ${PLUGIN}-debug

install: all
	@echo installing ${PLUGIN} plugin
	@mkdir -p ${DESTDIR}${PREFIX}/lib/zathura
	@cp -f ${PLUGIN}.so ${DESTDIR}${PREFIX}/lib/zathura

uninstall:
	@echo uninstalling ${PLUGIN} plugin
	@rm -f ${DESTDIR}${PREFIX}/lib/zathura/${PLUGIN}.so
	@rm -rf ${DESTDIR}${PREFIX}/lib/zathura

.PHONY: all options clean debug install uninstall
