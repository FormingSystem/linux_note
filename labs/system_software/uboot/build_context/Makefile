# 这三个名字用于观察 Make 变量的传播，不是 U-Boot 配置项。
demo_local := parent_only
export demo_exported := parent_value
demo_option ?= default_value

.PHONY: all proper spl
all: proper spl

proper spl:
	$(MAKE) -f stage.mk stage=$@ all
	@printf 'parent after %s: exported=%s\n' '$@' '$(demo_exported)'
