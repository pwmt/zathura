# See LICENSE file for license and copyright information
# zathura make config

# project
PROJECT = zathura

ZATHURA_VERSION_MAJOR = 0
ZATHURA_VERSION_MINOR = 3
ZATHURA_VERSION_REV = 3
# If the API changes, the API version and the ABI version have to be bumped.
ZATHURA_API_VERSION = 2
# If the ABI breaks for any reason, this has to be bumped.
ZATHURA_ABI_VERSION = 2
VERSION = ${ZATHURA_VERSION_MAJOR}.${ZATHURA_VERSION_MINOR}.${ZATHURA_VERSION_REV}

# version checks
# If you want to disable any of the checks, set *_VERSION_CHECK to 0.

# girara
GIRARA_VERSION_CHECK ?= 1
GIRARA_MIN_VERSION = 0.2.4
GIRARA_PKG_CONFIG_NAME = girara-gtk3
# glib
GLIB_VERSION_CHECK ?= 1
GLIB_MIN_VERSION = 2.32
GLIB_PKG_CONFIG_NAME = glib-2.0
# GTK
GTK_VERSION_CHECK ?= 1
GTK_MIN_VERSION = 3.0
GTK_PKG_CONFIG_NAME = gtk+-3.0

# pkg-config binary
PKG_CONFIG ?= pkg-config

# database
# To disable support for the sqlite backend set WITH_SQLITE to 0.
WITH_SQLITE ?= $(shell (${PKG_CONFIG} --atleast-version=3.5.9 sqlite3 && echo 1) || echo 0)

# synctex
# To disable support for synctex with libsynctex set WITH_SYNCTEX to 0.
WITH_SYNCTEX ?= $(shell (${PKG_CONFIG} synctex && echo 1) || echo 0)

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
DEPENDDIR ?= .depend
BUILDDIR ?= build
BUILDDIR_RELEASE ?= ${BUILDDIR}/release
BUILDDIR_DEBUG ?= ${BUILDDIR}/debug
BUILDDIR_GCOV ?= ${BUILDDIR}/gcov
BINDIR ?= bin

# plugin directory
PLUGINDIR ?= ${LIBDIR}/zathura
# locale directory
LOCALEDIR ?= ${PREFIX}/share/locale

# libs
GTK_INC ?= $(shell ${PKG_CONFIG} --cflags gtk+-3.0)
GTK_LIB ?= $(shell ${PKG_CONFIG} --libs gtk+-3.0)

GTHREAD_INC ?= $(shell ${PKG_CONFIG} --cflags gthread-2.0)
GTHREAD_LIB ?= $(shell ${PKG_CONFIG} --libs   gthread-2.0)

GMODULE_INC ?= $(shell ${PKG_CONFIG} --cflags gmodule-no-export-2.0)
GMODULE_LIB ?= $(shell ${PKG_CONFIG} --libs   gmodule-no-export-2.0)

GLIB_INC ?= $(shell ${PKG_CONFIG} --cflags glib-2.0)
GLIB_LIB ?= $(shell ${PKG_CONFIG} --libs glib-2.0)

GIRARA_INC ?= $(shell ${PKG_CONFIG} --cflags girara-gtk3)
GIRARA_LIB ?= $(shell ${PKG_CONFIG} --libs girara-gtk3)

ifneq (${WITH_SQLITE},0)
SQLITE_INC ?= $(shell ${PKG_CONFIG} --cflags sqlite3)
SQLITE_LIB ?= $(shell ${PKG_CONFIG} --libs sqlite3)
endif

ifneq (${WITH_MAGIC},0)
MAGIC_INC ?=
MAGIC_LIB ?= -lmagic
endif

ifneq ($(WITH_SYNCTEX),0)
SYNCTEX_INC ?= $(shell ${PKG_CONFIG} --cflags synctex)
SYNCTEX_LIB ?= $(shell ${PKG_CONFIG} --libs synctex)
endif

INCS = ${GIRARA_INC} ${GTK_INC} ${GTHREAD_INC} ${GMODULE_INC} ${GLIB_INC}
LIBS = ${GIRARA_LIB} ${GTK_LIB} ${GTHREAD_LIB} ${GMODULE_LIB} ${GLIB_LIB} -lpthread -lm

# pre-processor flags
CPPFLAGS += -D_FILE_OFFSET_BITS=64

# compiler flags
CFLAGS += -std=c11 -pedantic -Wall -Wno-format-zero-length -Wextra $(INCS)

# debug
DFLAGS ?= -g

# linker flags
LDFLAGS += -rdynamic

# compiler
CC ?= gcc

# strip
SFLAGS ?= -s

# msgfmt
MSGFMT ?= msgfmt

# gcov & lcov
GCOV_CFLAGS=-fprofile-arcs -ftest-coverage
GCOV_LDFLAGS=-fprofile-arcs
LCOV_OUTPUT=gcov
LCOV_EXEC=lcov
LCOV_FLAGS=--base-directory . --directory ${BUILDDIR_GCOV} --capture --rc \
					 lcov_branch_coverage=1 --output-file ${BUILDDIR_GCOV}/$(PROJECT).info
GENHTML_EXEC=genhtml
GENHTML_FLAGS=--rc lcov_branch_coverage=1 --output-directory ${LCOV_OUTPUT} ${BUILDDIR_GCOV}/$(PROJECT).info

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
