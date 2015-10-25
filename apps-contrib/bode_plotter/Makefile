#
# $Id: Makefile 1235 2014-02-21 16:44:10Z ales.bardorfer, luka.golinar $
#
# Contributed apps application specific apps
#

APP=$(notdir $(CURDIR:%/=%))

# Versioning system
BUILD_NUMBER ?= 0
REVISION ?= devbuild
VER:=$(shell grep version info/info.json | sed -E 's/^[^:]*:[^"]*"([^-]*)-.*/\1/')

CONTROLLER   = controller.so
CONTROLLERHF = controllerhf.so

#all: $(CONTROLLER) $(CONTROLLERHF)

all: $(CONTROLLERHF)

$(CONTROLLER):
	$(MAKE) -C src clean
	$(MAKE) -C src all OUT_NAME=$(CONTROLLER) CROSS_COMPILE=arm-linux-gnueabi-

$(CONTROLLERHF):
	$(MAKE) -C src clean
	$(MAKE) -C src all OUT_NAME=$(CONTROLLERHF)

#zip: $(CONTROLLER) $(CONTROLLERHF)

zip: $(CONTROLLERHF)
	-$(RM) target -r
	mkdir -p target/$(APP)
	#cp -r $(CONTROLLER) $(CONTROLLERHF) fpga.conf info index.html Makefile target/$(APP)
	cp -r $(CONTROLLERHF) fpga.conf info index.html Makefile target/$(APP)
	$(MAKE) -C src clean
	cd target; mkdir -p $(INSTALL_DIR); zip -r $(INSTALL_DIR)/$(APP)-$(VER)-$(BUILD_NUMBER)-$(REVISION).zip *
	-$(RM) -r target

clean:
	$(MAKE) -C src clean
	-$(RM) -r target
	-$(RM) *.so
