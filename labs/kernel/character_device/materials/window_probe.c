/* SPDX-License-Identifier: GPL-2.0 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void check(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "检查失败：%s\n", message);
		/* 进程退出时，系统关闭仍打开的描述符。 */
		exit(EXIT_FAILURE);
	}
}

int main(int argument_count, char **arguments)
{
	const unsigned char expected[8] = {'A', 'x', 'y', 'D', 'E', 'F', 0, 'Z'};
	unsigned char buffer[8];
	struct stat information;
	int descriptor;

	if (argument_count != 2) {
		fprintf(stderr, "用法：%s 字符设备路径\n", arguments[0]);
		return EXIT_FAILURE;
	}
	descriptor = open(arguments[1], O_RDWR);
	if (descriptor < 0) {
		perror("open");
		return EXIT_FAILURE;
	}
	check(fstat(descriptor, &information) == 0, "读取节点属性");
	check(S_ISCHR(information.st_mode), "输入必须是字符设备");
	check(read(descriptor, buffer, 1) == 0, "本次装载的窗口应为空");
	check(write(descriptor, "ABCDEF", 6) == 6, "写入六字节");
	check(read(descriptor, buffer, 1) == 0, "同一打开位置已经到达末尾");
	check(lseek(descriptor, 1, SEEK_SET) == 1, "回到位置一");
	check(write(descriptor, "xy", 2) == 2, "覆盖两个字节");
	check(lseek(descriptor, 7, SEEK_SET) == 7, "越过旧长度但不越过容量");
	check(write(descriptor, "Z", 1) == 1, "扩展并填补缺口");
	check(write(descriptor, "", 0) == 0, "零字节请求不需要剩余空间");
	check(lseek(descriptor, 0, SEEK_SET) == 0, "回到开头");
	check(read(descriptor, buffer, sizeof(buffer)) == 8, "读回完整内容");
	check(memcmp(buffer, expected, sizeof(expected)) == 0, "逐字节核对缺口零值");
	check(close(descriptor) == 0, "关闭设备");
	puts("窗口的覆盖、位置、EOF 与缺口填零符合预期");
	return EXIT_SUCCESS;
}
