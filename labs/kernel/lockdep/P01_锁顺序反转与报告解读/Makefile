obj-m += lockdep_cycle_demo.o

kernel_build ?= /lib/modules/$(shell uname -r)/build
module_dir := $(CURDIR)

.PHONY: all clean

all:
	$(MAKE) -C $(kernel_build) M=$(module_dir) modules

clean:
	$(MAKE) -C $(kernel_build) M=$(module_dir) clean
