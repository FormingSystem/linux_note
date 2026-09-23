// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

enum outcome { RETURNED, RETIRED, NO_SHARE, PUBLISHED_AFTER_RETIRE };
struct object_model {
    unsigned int shares[2]; /* 两个具名拥有者；地址别名不增加份额。 */
    unsigned int refs;
    bool alive, published;
};

static struct object_model create(unsigned int first, unsigned int second,
                                  bool published)
{
    return (struct object_model){{first, second}, first + second, true, published};
}
static enum outcome put_as(struct object_model *obj, unsigned int owner)
{
    assert(owner < 2);
    if (!obj->shares[owner])
        return NO_SHARE; /* 责任审计提前拒绝；真实kref不保存这张拥有者表。 */
    assert(obj->alive && obj->refs);
    --obj->shares[owner];
    if (--obj->refs)
        return RETURNED;
    obj->alive = false; /* 只标记逻辑退休，不释放C对象的实际存储。 */
    return obj->published ? PUBLISHED_AFTER_RETIRE : RETIRED;
}
static bool can_use_owned(const struct object_model *obj, unsigned int owner)
{
    assert(owner < 2);
    return obj->alive && obj->shares[owner] != 0;
}

int main(void)
{
    struct object_model shared = create(1, 1, false);
    assert(put_as(&shared, 0) == RETURNED);
    assert(shared.refs == 1 && can_use_owned(&shared, 1));
    assert(put_as(&shared, 0) == NO_SHARE);
    assert(shared.refs == 1); /* 数值为正，也不能替已归还者创造第二份。 */
    assert(put_as(&shared, 1) == RETIRED);
    puts("duplicate: positive count does not authorize another put");

    struct object_model observed = create(1, 0, false);
    // 在已有地址保护窗口内取快照；随后窗口结束，不把它带成一份引用。
    unsigned int snapshot = observed.refs;
    assert(put_as(&observed, 0) == RETIRED);
    assert(snapshot == 1 && !can_use_owned(&observed, 1));
    puts("snapshot: old positive value survives logical retirement");

    struct object_model bad_table = create(1, 0, true);
    // 槽0是表份额；错误路径先消费它，却让索引仍然可见。
    assert(put_as(&bad_table, 0) == PUBLISHED_AFTER_RETIRE);
    assert(bad_table.published && !bad_table.alive);
    puts("bad_table: retirement leaves a published address");

    struct object_model good_table = create(1, 0, true);
    good_table.published = false; /* 正确路径先在集合保护下摘除，再归还表份额。 */
    assert(put_as(&good_table, 0) == RETIRED);
    puts("good_table: unlink precedes returning the table share");

    struct object_model old_reader = create(1, 1, true);
    old_reader.published = false;
    assert(put_as(&old_reader, 0) == RETURNED);
    assert(!old_reader.published && can_use_owned(&old_reader, 1));
    assert(put_as(&old_reader, 1) == RETIRED);
    puts("old_reader: unpublished object remains owned until the reader returns");
    return 0;
}
