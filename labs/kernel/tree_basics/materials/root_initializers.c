#include <assert.h>
#include <stddef.h>
#include <stdio.h>

/* 使用仓库按固定提交保存的类型头；不包含或运行树更新算法。 */
#include "../../../../research/source_reading/linux/include/linux/rbtree_types.h"

struct job {
    int id;
    struct rb_node link;
};

int main(void)
{
    struct job item = { .id = 7 };
    struct rb_root root = RB_ROOT;
    struct rb_root_cached cached = RB_ROOT_CACHED;

    printf("initial: root_empty=%d cached_empty=%d\n",
           root.rb_node == NULL,
           cached.rb_root.rb_node == NULL && cached.rb_leftmost == NULL);

    /* 只构造地址关系以观察赋值；不是建立合法红黑树的插入方法。 */
    root.rb_node = &item.link;
    cached.rb_root.rb_node = &item.link;
    cached.rb_leftmost = &item.link;
    struct rb_root copied = root;

    root = RB_ROOT;
    assert(copied.rb_node == &item.link);
    printf("reset: root_empty=%d copy_kept=%d object_id=%d\n",
           root.rb_node == NULL, copied.rb_node == &item.link, item.id);

    /* 根是独立的值，复制并未复制节点，也没有建立回收规则。 */
    item.id = 9;
    assert(copied.rb_node == &item.link && item.id == 9);

    /* 仅重置内部普通根，会留下另一个尚未重置的入口。 */
    cached.rb_root = RB_ROOT;
    printf("partial reset: root_empty=%d leftmost_empty=%d\n",
           cached.rb_root.rb_node == NULL, cached.rb_leftmost == NULL);
    cached = RB_ROOT_CACHED;
    printf("full reset: root_empty=%d leftmost_empty=%d object_id=%d\n",
           cached.rb_root.rb_node == NULL, cached.rb_leftmost == NULL, item.id);

    /* 此处复合字面量的对象在当前块内仍然存在。 */
    struct rb_root *block_value = &(struct rb_root) { NULL, };
    block_value->rb_node = &item.link;
    assert(block_value->rb_node == copied.rb_node);
    copied = RB_ROOT;
    assert(block_value->rb_node == &item.link);
    printf("independent values: copy_empty=%d block_kept=%d\n",
           copied.rb_node == NULL, block_value->rb_node == &item.link);

    /* 返回前所有观察结束；外部不能继续使用这些自动对象的地址。 */
    return 0;
}
