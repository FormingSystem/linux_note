#include "common_bad.h"

void increment_count(void)
{
    /* 使用共享对象，使用者不应再提供一个独立定义。 */
    ++shared_count;
}
