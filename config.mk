# See LICENSE file for license and copyright information
# zathura make config

# project
PROJECT = zathura

ZATHURA_VERSION_MAJOR = 0
ZATHURA_VERSION_MINOR = 3
ZATHURA_VERSION_REV = 1
# If the API changes, the API version and the ABI version have to be bumped.
ZATHURA_API_VERSION = 2
# If the ABI breaks for any reason, this has to be bumped.
ZATHURA_ABI_VERSION = 2
VERSION = ${ZATHURA_VERSION_MAJOR}.${ZATHURA_VERSION_MINOR}.${ZATHURA_VERSION_REV}

# version checks
# If you want to disable any of the checks, set *_VERSION_CHECK to 0.

# girara
GIRARA_VERSION_CHECK ?= 1
GIRARA_MIN_VERSION = 0.2.3
GIRARA_PKG_CONFIG_NAME = girara-gtk3
# glib
GLIB_VERSION_CHECK ?= 1
GLIB_MIN_VERSION = 2.28
GLIB_PKG_CONFIG_NAME = glib-2.0
# GTK
GTK_VERSION_CHECK ?= 1
GTK_MIN_VERSION = 3.0
GTK_PKG_CONFIG_NAME = gtk+-3.0

# database
# To disable support for the sqlite backend set WITH_SQLITE to 0.
WITH_SQLITE ?= $(shell (pkg-config --atleast-version=3.5.9 sqlite3 && echo 1) || echo 0)

# synctex
# To use the embedded copy of the syntex parser set WITH_SYSTEM_SYNCTEX to 0.
WITH_SYSTEM_SYNCTEX ?= $(shell (pkg-config synctex && echo 1) || echo 0)

# mimetype detection
# To disable support for mimetype detction with libmagic set WITH_MAGIC to 0.
WITH_MAGIC ?= 1

# paths
PREFIX ?= /usr
MANPREFIX ?= ${PREFIX}/share/man
DESKTOPPREFIX ?= ${PREFIX}/share/applications
APPDATAPREFIX ?= ${PREFIX}/share/appdata
LIBDIR ?= ${PREFIX}/lib
INCLUDEDIR ?= ${PREFIX}/include
DBUSINTERFACEDIR ?= ${PREFIX}/share/dbus-1/interfaces
VIMFTPLUGINDIR ?= ${PREFIX}/share/vim/addons/ftplugin

# plugin directory
PLUGINDIR ?= ${LIBDIR}/zathura
# locale directory
LOCALEDIR ?= ${PREFIX}/share/locale

# libs
GTK_INC ?= $(shell pkg-config --cflags gtk+-3.0)
GTK_LIB ?= $(shell pkg-config --libs gtk+-3.0)

GTHREAD_INC ?= $(shell pkg-config --cflags gthread-2.0)
GTHREAD_LIB ?= $(shell pkg-config --libs   gthread-2.0)

GMODULE_INC ?= $(shell pkg-config --cflags gmodule-no-export-2.0)
GMODULE_LIB ?= $(shell pkg-config --libs   gmodule-no-export-2.0)

GLIB_INC ?= $(shell pkg-config --cflags glib-2.0)
GLIB_LIB ?= $(shell pkg-config --libs glib-2.0)

GIRARA_INC ?= $(shell pkg-config --cflags girara-gtk3)
GIRARA_LIB ?= $(shell pkg-config --libs girara-gtk3)

ifneq (${WITH_SQLITE},0)
SQLITE_INC ?= $(shell pkg-config --cflags sqlite3)
SQLITE_LIB ?= $(shell pkg-config --libs sqlite3)
endif

ifneq (${WITH_MAGIC},0)
MAGIC_INC ?=
MAGIC_LIB ?= -lmagic
endif

ifneq ($(WITH_SYSTEM_SYNCTEX),0)
SYNCTEX_INC ?= $(shell pkg-config --cflags synctex)
SYNCTEX_LIB ?= $(shell pkg-config --libs synctex)
else
ZLIB_INC ?= $(shell pkg-config --cflags zlib)
ZLIB_LIB ?= $(shell pkg-config --libs zlib)
endif

INCS = ${GIRARA_INC} ${GTK_INC} ${GTHREAD_INC} ${GMODULE_INC} ${GLIB_INC}
LIBS = ${GIRARA_LIB} ${GTK_LIB} ${GTHREAD_LIB} ${GMODULE_LIB} ${GLIB_LIB} -lpthread -lm

# flags
CFLAGS += -std=c99 -pedantic -Wall -Wno-format-zero-length -Wextra $(INCS)

# debug
DFLAGS ?= -g

# ld
LDFLAGS += -rdynamic

# compiler
CC ?= gcc

# strip
SFLAGS ?= -s

# msgfmt
MSGFMT ?= msgfmt

# valgrind
VALGRIND = valgrind
VALGRIND_ARGUMENTS = --tool=memcheck --leak-check=yes --leak-resolution=high \
	--show-reachable=yes --log-file=zathura-valgrind.log
VALGRIND_SUPPRESSION_FILE = zathura.suppression

# set to something != 0 if you want verbose build output
VERBOSE ?= 0

# colors
COLOR ?= 1

# dist
TARFILE = ${PROJECT}-${VERSION}.tar.gz
TARDIR = ${PROJECT}-${VERSION}
