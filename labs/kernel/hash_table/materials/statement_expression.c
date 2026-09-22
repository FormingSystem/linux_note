/* GNU C 的语句表达式；不是普通 ISO C 花括号块。 */
#include <stdio.h>

int main(void)
{
    int calls = 0;
    int result = ({
        int value = ++calls;
        value ? value + 10 : 0; /* 最后一个表达式的值成为整个表达式的值。 */
    });
    printf("result=%d calls=%d\n", result, calls);
    return result == 11 && calls == 1 ? 0 : 1;
}
