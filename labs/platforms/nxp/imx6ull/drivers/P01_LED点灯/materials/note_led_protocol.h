/* 一次write提交一条命令；解析器同时用于驱动与宿主边界检查。 */
#ifndef NOTE_LED_PROTOCOL_H
#define NOTE_LED_PROTOCOL_H
#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/types.h>
#else
#include <errno.h>
#include <stddef.h>
#endif

#define NOTE_LED_LIMIT 16U
#define NOTE_LED_COMMAND_MAX 7U

/* 只接受0..15的十进制索引、一个空格、on/off及可选的一个LF。 */
static inline int note_led_parse(const char *buf, size_t count,
                                 unsigned int *index, int *on)
{
    size_t pos = 0;
    unsigned int number;
    int value;

    if (!count)
        return 0;
    if (count > NOTE_LED_COMMAND_MAX)
        return -E2BIG;
    if (buf[count - 1] == '\n')
        --count;
    if (count < 4 || buf[0] < '0' || buf[0] > '9')
        return -EINVAL;
    number = (unsigned int)(buf[pos++] - '0');
    if (pos < count && buf[pos] >= '0' && buf[pos] <= '9') {
        if (!number)
            return -EINVAL; /* 不接受01之类的别名。 */
        number = number * 10 + (unsigned int)(buf[pos++] - '0');
    }
    if (number >= NOTE_LED_LIMIT || pos >= count || buf[pos++] != ' ')
        return -EINVAL;
    if (count - pos == 2 && buf[pos] == 'o' && buf[pos + 1] == 'n')
        value = 1;
    else if (count - pos == 3 && buf[pos] == 'o' &&
             buf[pos + 1] == 'f' && buf[pos + 2] == 'f')
        value = 0;
    else
        return -EINVAL;
    *index = number;
    *on = value;
    return 1;
}
#endif
