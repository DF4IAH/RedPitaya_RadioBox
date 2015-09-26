#
# $Id: Makefile 2015-09-24 18:23:00Z Ulrich Habel (DF4IAH) $
#
# Red Pitaya specific application Makefile for RadioBox.
#

CC=$(CROSS_COMPILE)gcc

VER:=$(shell cat ../info/info.json | grep version | sed -e 's/.*:\ *\"//' | sed -e 's/-.*//' | sed -e 's/",//')
BUILD_NUMBER ?= $(shell git rev-list HEAD --count)
REVISION ?= $(shell git log -n1 --pretty=%h)
VERSION ?= $(VER)-$(BUILD_NUMBER)

# Shared library
TARGET=controller.so

OBJS=main.o worker.o fpga.o fpga_awg.o calib.o generate.o


INCLUDE=

CFLAGS += -Wall -Werror -g -fPIC $(INCLUDE)
CFLAGS += -DVERSION=\"$(VERSION)\" -DREVISION=\"$(REVISION)\"

LDFLAGS=-shared

LIBS=-lm -lpthread


OUT_DIR=../
C_OUT_NAME=$(OUT_DIR)$(TARGET)


# Target with compilation rules to compile object from source files.
# It applies to all files ending with .o. During partial building only new object
# files are created for the source files (.c) which have newer timestamp then 
# objects (.o) files.
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# Main Makefile target 'all' - it iterates over all targets listed in $(TARGET)
# variable.
all: $(C_OUT_NAME)

# Makefile target with rules how to link executable for each target from $(TARGET)
# list.
$(C_OUT_NAME): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS)

clean:
	$(RM) $(C_OUT_NAME) $(OBJS)
