# See LICENSE file for license and copyright information

CHECK_INC ?= $(shell pkg-config --cflags check)
CHECK_LIB ?= $(shell pkg-config --libs check)

INCS += ${CHECK_INC} ${FIU_INC} -I../zathura
LIBS += ${CHECK_LIB} ${FIU_LIB} 

ZATHURA_RELEASE=../${BUILDDIR_RELEASE}/${BINDIR}/zathura
ZATHURA_DEBUG=../${BUILDDIR_DEBUG}/${BINDIR}/zathura
ZATHURA_GCOV=../${BUILDDIR_GCOV}/${BINDIR}/zathura

# valgrind
VALGRIND = valgrind
VALGRIND_ARGUMENTS = --tool=memcheck --leak-check=yes --leak-resolution=high \
	--show-reachable=yes --log-file=zathura-valgrind.log
VALGRIND_SUPPRESSION_FILE = zathura.suppression
