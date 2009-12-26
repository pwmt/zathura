# See LICENSE file for license and copyright information
# zathura make config

VERSION = 0.0.1

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

# libs
GTK_INC = $(shell pkg-config --cflags gtk+-2.0)
GTK_LIB = $(shell pkg-config --libs gtk+-2.0)

INCS = -I. -I/usr/include ${GTK_INC}
LIBS = -L/usr/lib -lc ${GTK_LIB}

# flags
CFLAGS = -std=c99 -pedantic -Wall $(INCS)
LDFLAGS = ${LIBS}

# debug
DFLAGS = -g

# compiler
CC = gcc
