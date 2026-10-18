obj-m += loglab.o
kdir ?= /lib/modules/$(shell uname -r)/build
module_dir := $(CURDIR)

.PHONY: all clean
all:
	$(MAKE) -C $(kdir) M=$(module_dir) modules

clean:
	$(MAKE) -C $(kdir) M=$(module_dir) clean
