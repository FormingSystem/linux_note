#include <stdio.h>
#include "common_bad.h"

/* 此文件是对象的实际所有者；修正后仍保留这一份定义。 */
int shared_count;

int main(void)
{
    increment_count();
    printf("shared_count=%d\n", shared_count);
    return shared_count == 1 ? 0 : 1;
}
