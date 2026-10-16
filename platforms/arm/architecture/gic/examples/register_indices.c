/* 普通 SPI 的寄存器索引算例；仅做整数计算，不访问硬件。 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char *end = NULL;
    long parsed;
    unsigned int intid;

    if (argc != 2) {
        fprintf(stderr, "用法: %s INTID（十进制普通 SPI：32～1019）\n", argv[0]);
        return EXIT_FAILURE;
    }
    errno = 0;
    parsed = strtol(argv[1], &end, 10);
    if (errno != 0 || end == argv[1] || *end != '\0' ||
        parsed < 32 || parsed > 1019) {
        fputs("本例只计算 GICv3.0 普通 SPI：32～1019 的十进制整数\n", stderr);
        return EXIT_FAILURE;
    }
    intid = (unsigned int)parsed;

    /* 商选择寄存器，余数选择同一寄存器中的位或字段。 */
    printf("使能: GICD_ISENABLER%u, bit %u\n", intid / 32, intid % 32);
    printf("触发: GICD_ICFGR%u, bits %u:%u\n",
           intid / 16, 2 * (intid % 16) + 1, 2 * (intid % 16));
    printf("优先级: 字节 %u; GICD_IPRIORITYR%u, bits %u:%u\n",
           intid, intid / 4, 8 * (intid % 4) + 7, 8 * (intid % 4));
    printf("目标: GICD_IROUTER%u\n", intid);
    return EXIT_SUCCESS;
}
