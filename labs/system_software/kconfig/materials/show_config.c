/* 本程序展示生成头，不把 m 配置假装成实际可加载模块。 */
#include <stdio.h>
#include "include/generated/autoconf.h"

int main(void)
{
#if defined(CONFIG_DRIVER)
    puts("DRIVER=y");
#elif defined(CONFIG_DRIVER_MODULE)
    puts("DRIVER=m");
#else
    puts("DRIVER=n");
#endif

#ifdef CONFIG_BUFFER_SIZE
    printf("BUFFER_SIZE=%d\n", CONFIG_BUFFER_SIZE);
#endif
    printf("DEFAULT_ORDER=%d COPIED_DEFAULT=%d\n",
           CONFIG_DEFAULT_ORDER, CONFIG_COPIED_DEFAULT);
    return 0;
}
