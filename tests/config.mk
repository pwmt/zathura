# See LICENSE file for license and copyright information

CHECK_INC ?= $(shell pkg-config --cflags check)
CHECK_LIB ?= $(shell pkg-config --libs check)

LIBS += ${CHECK_LIB}
