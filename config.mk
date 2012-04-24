# See LICENSE file for license and copyright information
# zathura make config

ZATHURA_VERSION_MAJOR = 0
ZATHURA_VERSION_MINOR = 1
ZATHURA_VERSION_REV = 2
# If the API changes, the API version and the ABI version have to be bumped.
ZATHURA_API_VERSION = 1
# If the ABI breaks for any reason, this has to be bumped.
ZATHURA_ABI_VERSION = 1
VERSION = ${ZATHURA_VERSION_MAJOR}.${ZATHURA_VERSION_MINOR}.${ZATHURA_VERSION_REV}

# the GTK+ version to use
# note: zathura with GTK+ 3 is broken!
ZATHURA_GTK_VERSION ?= 2

# minimum required zathura version
# If you want to disable the check, set GIRARA_VERSION_CHECK to 0.
GIRARA_MIN_VERSION = 0.1.2
GIRARA_VERSION_CHECK ?= $(shell pkg-config --atleast-version=$(GIRARA_MIN_VERSION) girara-gtk${ZATHURA_GTK_VERSION}; echo $$?)

# database
# To disable support for the sqlite backend set WITH_SQLITE to 0.
WITH_SQLITE ?= $(shell (pkg-config --exists sqlite3 && echo 1) || echo 0)

# paths
PREFIX ?= /usr
MANPREFIX ?= ${PREFIX}/share/man
DESKTOPPREFIX ?= ${PREFIX}/share/applications
LIBDIR ?= ${PREFIX}/lib
INCLUDEDIR ?= ${PREFIX}/include

# plugin directory
PLUGINDIR ?= ${LIBDIR}/zathura
# locale directory
LOCALEDIR ?= ${PREFIX}/share/locale

# rst2man
RSTTOMAN ?= /usr/bin/rst2man

# libs
GTK_INC ?= $(shell pkg-config --cflags gtk+-${ZATHURA_GTK_VERSION}.0)
GTK_LIB ?= $(shell pkg-config --libs gtk+-${ZATHURA_GTK_VERSION}.0)

GTHREAD_INC ?= $(shell pkg-config --cflags gthread-2.0)
GTHREAD_LIB ?= $(shell pkg-config --libs   gthread-2.0)

GIRARA_INC ?= $(shell pkg-config --cflags girara-gtk${ZATHURA_GTK_VERSION})
GIRARA_LIB ?= $(shell pkg-config --libs girara-gtk${ZATHURA_GTK_VERSION})

ifneq (${WITH_SQLITE},0)
SQLITE_INC ?= $(shell pkg-config --cflags sqlite3)
SQLITE_LIB ?= $(shell pkg-config --libs sqlite3)
endif

#set it to an empty value if you don't need to link against ld for dlopen and friends
DL_LIB ?= -ldl

INCS = ${GIRARA_INC} ${GTK_INC} ${GTHREAD_INC}
LIBS = ${GIRARA_LIB} ${GTK_LIB} ${GTHREAD_LIB} ${DL_LIB} -lpthread -lm

# flags
CFLAGS += -std=c99 -pedantic -Wall -Wno-format-zero-length -Wextra $(INCS)

ifeq (,$(findstring -DZATHURA_PLUGINDIR,${CPPFLAGS}))
CPPFLAGS += -DZATHURA_PLUGINDIR=\"${PLUGINDIR}\"
endif
ifeq (,$(findstring -DGETTEXT_PACKAGE,${CPPFLAGS}))
CPPFLAGS += -DGETTEXT_PACKAGE=\"${PROJECT}\"
endif
ifeq (,$(findstring -DLOCALEDIR,${CPPFLAGS}))
CPPFLAGS += -DLOCALEDIR=\"${LOCALEDIR}\"
endif

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

