# See LICENSE file for license and copyright information
# zathura make config

VERSION = 0.0.8.1

# paths
PREFIX ?= /usr
MANPREFIX ?= ${PREFIX}/share/man

# libs
GTK_INC = $(shell pkg-config --cflags gtk+-2.0)
GTK_LIB = $(shell pkg-config --libs gtk+-2.0 gthread-2.0)

GIRARA_INC = $(shell pkg-config --cflags girara-gtk2)
GIRARA_LIB = $(shell pkg-config --libs girara-gtk2)

INCS = -I. -I/usr/include ${GIRARA_INC} ${GTK_INC}
LIBS = -lc ${GIRARA_LIB} ${GTK_LIB} -lpthread -lm -ldl

# flags
CFLAGS += -std=c99 -pedantic -Wall -Wno-format-zero-length $(INCS)

# debug
DFLAGS = -g

# ld
LDFLAGS += -rdynamic

# compiler
CC ?= gcc

# strip
SFLAGS ?= -s
