#include <stdbool.h>

#ifdef __CHECKER__
#define __bitwise  __attribute__((bitwise))
#define __force    __attribute__((force))
#define __nocast   __attribute__((nocast))
#define __safe     __attribute__((safe))
#define __private  __attribute__((noderef))
#define ACCESS_PRIVATE(object, member) \
    (*((typeof((object)->member) __force *)&(object)->member))
#else
#define __bitwise
#define __force
#define __nocast
#define __safe
#define __private
#define ACCESS_PRIVATE(object, member) ((object)->member)
#endif

typedef unsigned int __bitwise record_flags_t;

#define RECORD_FLAG_COMPRESSED \
    ((__force record_flags_t)(1U << 0))
#define RECORD_FLAG_CHECKSUM \
    ((__force record_flags_t)(1U << 1))
#define RECORD_FLAG_MASK_RAW ((1U << 0) | (1U << 1))

struct wire_record_header {
    unsigned int raw_flags;
};

static bool decode_record_flags(const struct wire_record_header *header,
                                record_flags_t *out_flags)
{
    unsigned int raw_flags = header->raw_flags;

    /* 先完成运行时范围证明，拒绝协议没有定义的标志位。 */
    if (raw_flags & ~RECORD_FLAG_MASK_RAW)
        return false;

    /* 范围已经验证，才在这个边界上执行受控类型转换。 */
    *out_flags = (__force record_flags_t)raw_flags;
    return true;
}

static bool record_is_compressed(record_flags_t flags)
{
    /* 两个操作数属于同一个受限类型，可以执行约定的位运算。 */
    return (flags & RECORD_FLAG_COMPRESSED) != 0;
}

static record_flags_t decode_record_flags_without_force(
    const struct wire_record_header *header)
{
    /* Sparse应报告普通整数被隐式当成受限类型。 */
    return header->raw_flags;
}

static record_flags_t decode_record_flags_bad_force(
    const struct wire_record_header *header)
{
    /* 虽然不再产生类型诊断，却错误地跳过了未知标志位检查。 */
    return (__force record_flags_t)header->raw_flags;
}

typedef unsigned long long __nocast record_bytes_t;

static int record_bytes_bad(record_bytes_t bytes)
{
    /* Sparse应报告从带nocast限定的宽整数转换为int。 */
    return bytes;
}

static bool record_bytes_to_uint(record_bytes_t bytes,
                                 unsigned int *out)
{
    /* 先证明目标类型能够保存当前数值。 */
    if (bytes > ~0U)
        return false;

    /* 范围证明成立后，显式放宽这一次转换。 */
    *out = (__force unsigned int)bytes;
    return true;
}

static unsigned long long record_bytes_with_header(record_bytes_t bytes)
{
    /* __nocast允许与普通整数混合，结果也可能丢失该限定。 */
    return bytes + 64U;
}

struct record_body {
    unsigned int length;
};

static unsigned int record_length_checked(const struct record_body *record)
{
    const struct record_body * __safe checked_record;

    /* 功能路径先完成真实的空指针检查。 */
    if (!record)
        return 0;

    /* 从这里开始，checked_record记录已经成立的不为空不变量。 */
    checked_record = record;
    return checked_record->length;
}

static unsigned int record_length_redundant(
    const struct record_body * __safe record)
{
    /* Sparse应提醒：该形参已经声明为safe，却又被条件测试。 */
    if (!record)
        return 0;

    return record->length;
}

enum record_cache_state {
    RECORD_CACHE_EMPTY,
    RECORD_CACHE_READY,
};

struct record_cache {
    unsigned int __private state;
};

static void record_cache_init(struct record_cache *cache)
{
    ACCESS_PRIVATE(cache, state) = RECORD_CACHE_EMPTY;
}

static bool record_cache_publish(struct record_cache *cache)
{
    if (ACCESS_PRIVATE(cache, state) != RECORD_CACHE_EMPTY)
        return false;

    ACCESS_PRIVATE(cache, state) = RECORD_CACHE_READY;
    return true;
}

static bool record_cache_is_ready(struct record_cache *cache)
{
    return ACCESS_PRIVATE(cache, state) == RECORD_CACHE_READY;
}

static void record_cache_reset(struct record_cache *cache)
{
    ACCESS_PRIVATE(cache, state) = RECORD_CACHE_EMPTY;
}

static void record_cache_break_boundary(struct record_cache *cache)
{
    /* Sparse应报告对private成员的普通访问。 */
    cache->state = RECORD_CACHE_READY;
}
