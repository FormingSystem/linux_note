# 本机实验的默认值；交叉编译时在命令行覆盖。
kernel_build ?= /lib/modules/$(shell uname -r)/build
module_dir := $(CURDIR)

.PHONY: all clean
all:
	$(MAKE) -C "$(kernel_build)" M="$(module_dir)" modules

clean:
	$(MAKE) -C "$(kernel_build)" M="$(module_dir)" clean
