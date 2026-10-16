#include <stdio.h>
#include <string.h>

/* 宏常量 NOTE_CAPACITY：本模型每份记录的最大容量，单位为字节。 */
#define NOTE_CAPACITY 8

struct note_state {
    unsigned char data[NOTE_CAPACITY];
    size_t length;
};

static size_t copy_model(unsigned char *to, const unsigned char *from,
                         size_t count, size_t budget)
{
    size_t done = budget < count ? budget : count;
    /* 只模拟这里声明的部分复制结果，不代替真实用户访问。 */
    memcpy(to, from, done);
    memset(to + done, 0, count - done);
    return count - done;
}

static int replace_direct(struct note_state *state,
                          const unsigned char *input, size_t count,
                          size_t budget)
{
    if (count > NOTE_CAPACITY)
        return -2;
    if (count == 0)
        return 0;
    /* 故意保留错误设计：失败时共享数据已经被部分覆盖。 */
    if (copy_model(state->data, input, count, budget))
        return -1;
    state->length = count;
    return (int)count;
}

static int replace_staged(struct note_state *state,
                          const unsigned char *input, size_t count,
                          size_t budget)
{
    unsigned char temporary[NOTE_CAPACITY];

    if (count > NOTE_CAPACITY)
        return -2;
    if (count == 0)
        return 0;
    if (copy_model(temporary, input, count, budget))
        return -1;
    /* 真正驱动在这里加锁、复查状态，再一起发布数据与长度。 */
    memcpy(state->data, temporary, count);
    state->length = count;
    return (int)count;
}

static void show(const char *name, int result, const struct note_state *state)
{
    printf("%s: result=%d length=%zu bytes=", name, result, state->length);
    for (size_t i = 0; i < state->length; ++i)
        printf("%02x", (unsigned int)state->data[i]);
    putchar('\n');
}

int main(void)
{
    struct note_state direct = { "ABCDEF", 6 };
    struct note_state staged = direct;
    int result;

    result = replace_direct(&direct, (const unsigned char *)"WXYZ", 4, 2);
    show("direct failure", result, &direct);
    result = replace_staged(&staged, (const unsigned char *)"WXYZ", 4, 2);
    show("staged failure", result, &staged);
    result = replace_staged(&staged, (const unsigned char *)"OK", 2, 2);
    show("staged success", result, &staged);
    return 0;
}
