#include <stdio.h>
#include "settings.h"

int main(void)
{
    /* 打印头文件的编译期取值，观察头文件依赖是否触发重建。 */
    printf("build_value=%d\n", BUILD_VALUE);
    return 0;
}
