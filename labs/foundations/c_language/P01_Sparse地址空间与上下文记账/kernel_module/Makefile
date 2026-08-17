ifneq ($(KERNELRELEASE),)
obj-m += sparse_kbuild_probe.o
else
KERNEL_BUILD ?= /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)

.PHONY: doctor build check-c1 check-c2 check-good check-bad clean

doctor:
	@test -f "$(KERNEL_BUILD)/Makefile" || { \
		echo "找不到内核构建目录: $(KERNEL_BUILD)" >&2; \
		exit 1; \
	}
	@command -v sparse
	@sparse --version
	@echo "KERNEL_BUILD=$(KERNEL_BUILD)"

build:
	$(MAKE) -C "$(KERNEL_BUILD)" M="$(MODULE_DIR)" modules

check-c1:
	$(MAKE) -C "$(KERNEL_BUILD)" M="$(MODULE_DIR)" C=1 V=1 modules

check-c2:
	$(MAKE) -C "$(KERNEL_BUILD)" M="$(MODULE_DIR)" C=2 V=1 modules

check-good:
	$(MAKE) -C "$(KERNEL_BUILD)" M="$(MODULE_DIR)" C=2 \
		CF="-Wcontext" modules

check-bad:
	$(MAKE) -C "$(KERNEL_BUILD)" M="$(MODULE_DIR)" C=2 \
		CF="-Wcontext -DBUILD_BAD_ADDRESS" modules

clean:
	$(MAKE) -C "$(KERNEL_BUILD)" M="$(MODULE_DIR)" clean
endif
