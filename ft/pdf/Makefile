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
	@${CC} -shared ${LDFLAGS} -o ${PLUGIN}-debug.so $(DOBJECTS) ${LIBS}

clean:
	@rm -rf ${OBJECTS} ${DOBJECTS} $(PLUGIN).so ${PLUGIN}-debug.so

debug: options ${PLUGIN}-debug
	@make -C debug
