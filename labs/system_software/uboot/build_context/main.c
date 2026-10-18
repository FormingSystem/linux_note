#include <stdio.h>

const char *board_stage(void);

int main(void)
{
#ifdef DEMO_SPL
    const char *compiled_for = "spl";
#else
    const char *compiled_for = "proper";
#endif
    /* 同一源文件在两个目录各编译一次，打印编译与链接选择。 */
    printf("compiled=%s linked=%s\n", compiled_for, board_stage());
    return 0;
}
