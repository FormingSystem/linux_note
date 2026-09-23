/* GNU C 宿主实验：使用保存的固定宏；不调用树算法或父色指针编码。 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "../../../../research/source_reading/linux/include/linux/rbtree_types.h"
#include "../../../../research/source_reading/linux/include/linux/container_of.h"

struct job {
    unsigned int id;
    struct rb_node by_deadline;
    unsigned long deadline;
    struct rb_node by_id;
};

int main(void)
{
    struct job item = { .id = 7, .deadline = 40 };
    struct rb_node *deadline_node = &item.by_deadline;
    struct rb_node *id_node = &item.by_id;
    struct job *from_deadline = container_of(deadline_node, struct job, by_deadline);
    struct job *from_id = container_of(id_node, struct job, by_id);
    const struct rb_node *read_node = &item.by_id;
    const struct job *read_owner = container_of_const(read_node, struct job, by_id);
    struct job copy = item;

    /* 偏移由当前编译器布局决定，不能把某台机器的数值写死。 */
    size_t deadline_offset = offsetof(struct job, by_deadline);
    size_t id_offset = offsetof(struct job, by_id);
    assert(deadline_offset > 0 && id_offset > deadline_offset);
    assert(from_deadline == &item && from_id == &item && read_owner == &item);
    printf("offsets: deadline=%zu id=%zu\n", deadline_offset, id_offset);
    printf("same owner: %d; keys: %lu,%u\n",
           from_deadline == from_id, from_deadline->deadline, from_id->id);

    /* 成员类型相同不代表成员身份相同；这里只算整数，不构造错误指针。 */
    _Static_assert(__same_type(item.by_deadline, item.by_id), "same member type");
    printf("wrong member would shift origin by %zu bytes\n",
           id_offset - deadline_offset);

    /* 复制结构体不会让现有成员地址自动改指新对象。 */
    copy.id = 8;
    assert(container_of(id_node, struct job, by_id) == &item);
    assert(container_of(&copy.by_id, struct job, by_id) == &copy);
    printf("saved node owner id=%u; copied object id=%u\n", from_id->id, copy.id);

    /* 普通宏会丢掉 const；本例仅检查类型，不借此修改只读对象。 */
    _Static_assert(__same_type(container_of(read_node, struct job, by_id),
                              (struct job *)0), "ordinary result");
    _Static_assert(__same_type(container_of_const(read_node, struct job, by_id),
                              (const struct job *)0), "const result");
    puts("const owner preserved by container_of_const");
    return 0;
}
