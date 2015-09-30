#
# $Id: Makefile 2015-09-24 18:23:00Z Ulrich Habel (DF4IAH) $
#
# Red Pitaya specific application Makefile for RadioBox.
#

APP=$(notdir $(CURDIR:%/=%))

FPGA=fpga_rb_0.94.bit
FPGACFG=fpga.conf
TARGET=controller.so
PDF=doc/RedPitaya_RadioBox_Development_Manual.pdf

# Versioning system
#BUILD_NUMBER ?= 0
#REVISION ?= devbuild

# alternate versioning system by DF4IAH
BUILD_NUMBER ?= $(shell git rev-list HEAD --count)
REVISION ?= $(shell git log -n1 --pretty=%h)

VER:=$(shell cat info/info.json | grep version | sed -e 's/.*:\ *\"//' | sed -e 's/-.*//' | sed -e 's/",//')
VERSION ?= $(VER)-$(BUILD_NUMBER)


INSTALL_DIR ?= ../


all: $(TARGET) doc
	$(MAKE) -C src all


src: $(TARGET)

$(TARGET):
	$(MAKE) -C src all


doc: $(PDF)

$(PDF):
	$(MAKE) -C doc all


zip: $(TARGET) $(PDF)
	mkdir -p target/$(APP)/bin
	mkdir -p target/$(APP)/doc
	cp -r $(TARGET) $(FPGA) $(FPGACFG) bin info index.html Makefile target/$(APP)
	cp -r src target/$(APP)
	cp -r doc/*.pdf doc/html target/$(APP)/doc
	find target -iname .svn -delete
	sed -i target/$(APP)/info/info.json -e 's/REVISION/$(REVISION)/'
	sed -i target/$(APP)/info/info.json -e 's/BUILD_NUMBER/$(BUILD_NUMBER)/'
	cd target; zip -r $(INSTALL_DIR)/$(APP)-$(VERSION)-$(REVISION).zip *
	cd ..
	$(RM) -r target


clean:
	-$(MAKE) -C src clean
	-$(MAKE) -C doc clean
	-$(RM) -r target
	-$(RM) *.zip
