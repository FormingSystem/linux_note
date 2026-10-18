ifeq ($(filter $(stage),proper spl),)
$(error stage must be proper or spl)
endif

# 先打印继承结果，再修改本进程的变量，不会反向修改父 make。
$(info child $(stage): local=$(demo_local) exported=$(demo_exported) option=$(demo_option))
demo_exported := child_value
build_dir := out/$(stage)
stage_source := $(stage).c
stage_cppflags :=
ifeq ($(stage),spl)
stage_cppflags := -DDEMO_SPL
endif

.PHONY: all run
all: $(build_dir)/demo.exe run

$(build_dir):
	mkdir -p $@

$(build_dir)/main.o: main.c | $(build_dir)
	$(CC) -std=c11 -Wall -Wextra -Werror $(stage_cppflags) -c $< -o $@

$(build_dir)/stage.o: $(stage_source) | $(build_dir)
	$(CC) -std=c11 -Wall -Wextra -Werror -c $< -o $@

$(build_dir)/demo.exe: $(build_dir)/main.o $(build_dir)/stage.o
	$(CC) $^ -o $@

run: $(build_dir)/demo.exe
	./$(build_dir)/demo.exe
