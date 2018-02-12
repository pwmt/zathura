# See LICENSE file for license and copyright information
# zathura make config

# project
PROJECT = zathura

ZATHURA_VERSION_MAJOR = 0
ZATHURA_VERSION_MINOR = 3
ZATHURA_VERSION_REV = 8
# If the API changes, the API version and the ABI version have to be bumped.
ZATHURA_API_VERSION = 3
# If the ABI breaks for any reason, this has to be bumped.
ZATHURA_ABI_VERSION = 4
VERSION = ${ZATHURA_VERSION_MAJOR}.${ZATHURA_VERSION_MINOR}.${ZATHURA_VERSION_REV}

# version checks
# If you want to disable any of the checks, set *_VERSION_CHECK to 0.

# girara
GIRARA_VERSION_CHECK ?= 1
GIRARA_MIN_VERSION = 0.2.8
GIRARA_PKG_CONFIG_NAME = girara-gtk3
# glib
GLIB_VERSION_CHECK ?= 1
GLIB_MIN_VERSION = 2.50
GLIB_PKG_CONFIG_NAME = glib-2.0
# GTK
GTK_VERSION_CHECK ?= 1
GTK_MIN_VERSION = 3.22
GTK_PKG_CONFIG_NAME = gtk+-3.0

# pkg-config binary
PKG_CONFIG ?= pkg-config

# glib-compile-resources
GLIB_COMPILE_RESOURCES ?= glib-compile-resources

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
APPDATAPREFIX ?= ${PREFIX}/share/metainfo
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
ifeq (${GTK_INC}-${GTK_LIB},-)
PKG_CONFIG_LIBS += gtk+-3.0
else
INCS += ${GTK_INC}
LIBS += ${GTK_LIB}
endif

ifeq (${GLIB_INC}-${GLIB_LIB},-)
PKG_CONFIG_LIBS += gthread-2.0 gmodule-no-export-2.0 glib-2.0
else
INCS += ${GLIB_INC}
LIBS += ${GLIB_LIB}
endif

ifeq (${GIRARA_INC}-${GIRARA_LIB},-)
PKG_CONFIG_LIBS += girara-gtk3
else
INCS += ${GIRARA_INC}
LIBS += ${GIRARA_LIB}
endif

ifneq (${WITH_SQLITE},0)
ifeq (${SQLITE_INC}-${SQLITE_LIB},-)
PKG_CONFIG_LIBS += sqlite3
else
INCS += ${SQLITE_INC}
LIBS += ${SQLITE_LIB}
endif
endif

ifneq (${WITH_MAGIC},0)
MAGIC_INC ?=
MAGIC_LIB ?= -lmagic

INCS += ${MAGIC_INC}
LIBS += ${MAGIC_LIB}
endif

ifneq ($(WITH_SYNCTEX),0)
ifeq (${SYNCTEX_INC}-${SYNCTEX_LIB},-)
PKG_CONFIG_LIBS += synctex
else
INCS += ${SYNCTEX_INC}
LIBS += ${SYNCTEX_LIB}
endif
endif

ifneq (${PKG_CONFIG_LIBS},)
INCS += $(shell ${PKG_CONFIG} --cflags ${PKG_CONFIG_LIBS})
LIBS += $(shell ${PKG_CONFIG} --libs ${PKG_CONFIG_LIBS})
endif
LIBS += -lpthread -lm

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

# gettext package name
GETTEXT_PACKAGE ?= ${PROJECT}

# colors
COLOR ?= 1

# dist
TARFILE = ${PROJECT}-${VERSION}.tar.gz
TARDIR = ${PROJECT}-${VERSION}
