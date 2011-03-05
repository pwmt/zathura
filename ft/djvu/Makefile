# See LICENSE file for license and copyright information

include config.mk

PLUGIN   = djvu
SOURCE   = djvu.c
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
	@${LD} -shared ${LDFLAGS} -o ${PLUGIN}.so $(OBJECTS)

${PLUGIN}-debug: ${DOBJECTS}
	@echo LD $@
	@${LD} -shared ${LDFLAGS} -o ${PLUGIN}-debug.so $(DOBJECTS)

clean:
	@rm -rf ${OBJECTS} ${DOBJECTS} $(PLUGIN).so ${PLUGIN}-debug.so

debug: options ${PLUGIN}-debug
	@make -C debug
