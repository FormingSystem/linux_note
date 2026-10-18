.DEFAULT_GOAL := all
VPATH := src
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Werror -MMD -MP

.PHONY: all report
all: demo.exe

demo.exe: main.o
	$(CC) $^ -o $@

main.o: main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

report:
	@printf 'report target ran\n'

# 缺少首次编译才生成的依赖文件不是错误。
-include main.d
