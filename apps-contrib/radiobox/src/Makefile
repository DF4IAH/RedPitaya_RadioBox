#
# $Id: Makefile 2015-09-24 18:23:00Z Ulrich Habel (DF4IAH) $
#
# Red Pitaya specific application Makefile for RadioBox.
#

CROSS_COMPILE ?= arm-linux-gnueabihf-
CC=$(CROSS_COMPILE)gcc
RM=rm

OBJECTS=main.o worker.o cb_ws.o fpga.o calib.o

CFLAGS+= -Wall -Werror -g -fPIC $(INCLUDE)
LDFLAGS=-shared

OUT_DIR = ..
OUT_NAME ?= controllerhf.so
CONTROLLER = $(OUT_DIR)/$(OUT_NAME)

all: $(CONTROLLER)

$(CONTROLLER): $(OBJECTS)
	$(CC) -o $(CONTROLLER) $(OBJECTS) $(CFLAGS) $(LDFLAGS)

clean:
	-$(RM) -f $(OBJECTS)
