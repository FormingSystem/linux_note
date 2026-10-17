obj-m += late_and_preempted_reader.o

KDIR ?= /lib/modules/$(shell uname -r)/build
module_dir := $(CURDIR)

.PHONY: all clean

all:
	$(MAKE) -C "$(KDIR)" M="$(module_dir)" modules

clean:
	$(MAKE) -C "$(KDIR)" M="$(module_dir)" clean
