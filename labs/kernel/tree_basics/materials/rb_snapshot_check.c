// SPDX-License-Identifier: MIT
/* 有界快照模型：整数索引只指向已知存活数组，不检查任意内核指针。 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CAPACITY 8
#define NIL (-1)
struct node {
    bool live, red;
    int key, parent, left, right;
    unsigned int score, subtree_max;
};
struct tree {
    struct node pool[CAPACITY];
    int root, first;
    unsigned int count;
};
struct result {
    const char *error;
    unsigned int count, black, maximum;
    int first;
};

static struct result failure(const char *error)
{
    return (struct result){.error=error, .first=NIL};
}

static struct result walk(const struct tree *tree, int index, int parent,
                          bool parent_red, bool has_low, int low,
                          bool has_high, int high, bool seen[CAPACITY])
{
    if (index == NIL)
        return (struct result){.black=1, .first=NIL}; /* 黑计数包含 NIL。 */
    if (index < 0 || index >= CAPACITY || !tree->pool[index].live)
        return failure("invalid-index");
    if (seen[index])
        return failure("repeated-node");
    seen[index] = true; /* 每个槽最多访问一次，环和共享孩子都会被拒绝。 */
    const struct node *node = &tree->pool[index];
    if (node->parent != parent)
        return failure("parent");
    if ((has_low && node->key <= low) || (has_high && node->key >= high))
        return failure("key-order");
    if (parent == NIL && node->red)
        return failure("root-red");
    if (parent_red && node->red)
        return failure("red-red");
    struct result left = walk(tree, node->left, index, node->red,
                              has_low, low, true, node->key, seen);
    if (left.error)
        return left;
    struct result right = walk(tree, node->right, index, node->red,
                               true, node->key, has_high, high, seen);
    if (right.error)
        return right;
    if (left.black != right.black)
        return failure("black-height");
    unsigned int maximum = node->score;
    if (left.maximum > maximum)
        maximum = left.maximum;
    if (right.maximum > maximum)
        maximum = right.maximum;
    if (node->subtree_max != maximum)
        return failure("summary");
    return (struct result){.count=1+left.count+right.count,
        .black=left.black+(node->red ? 0U : 1U), .maximum=maximum,
        .first=left.first == NIL ? index : left.first};
}

static struct result inspect(const struct tree *tree)
{
    bool seen[CAPACITY] = {false};
    struct result result = walk(tree, tree->root, NIL, false,
                                false, 0, false, 0, seen);
    if (result.error)
        return result;
    if (result.count != tree->count)
        return failure("count");
    if (result.first != tree->first)
        return failure("cached-first");
    return result;
}

static struct tree baseline(void)
{
    return (struct tree){
        .pool={
            {.live=true,.red=false,.key=20,.parent=NIL,.left=1,.right=2,.score=5,.subtree_max=9},
            {.live=true,.red=true,.key=10,.parent=0,.left=NIL,.right=NIL,.score=2,.subtree_max=2},
            {.live=true,.red=true,.key=30,.parent=0,.left=NIL,.right=NIL,.score=9,.subtree_max=9}
        }, .root=0,.first=1,.count=3
    };
}

static int expect(const char *name, const struct tree *tree, const char *expected)
{
    struct result result = inspect(tree);
    const char *actual = result.error ? result.error : "ok";
    printf("%s: %s\n", name, actual);
    return strcmp(actual, expected) != 0;
}

int main(void)
{
    int failed = 0;
    struct tree tree = baseline();
    failed += expect("valid", &tree, "ok");
    tree.pool[1].key = 21;
    failed += expect("wrong key", &tree, "key-order");
    tree = baseline(); tree.pool[1].parent = 2;
    failed += expect("wrong parent", &tree, "parent");
    tree = baseline(); tree.pool[0].red = true;
    failed += expect("red root", &tree, "root-red");
    tree = baseline(); tree.pool[3] = (struct node){.live=true,.red=true,.key=5,
        .parent=1,.left=NIL,.right=NIL,.score=1,.subtree_max=1};
    tree.pool[1].left = 3; tree.count = 4; tree.first = 3;
    failed += expect("red child", &tree, "red-red");
    tree = baseline(); tree.pool[1].red = false;
    failed += expect("unequal paths", &tree, "black-height");
    tree = baseline(); tree.first = 2;
    failed += expect("stale cache", &tree, "cached-first");
    tree = baseline(); tree.pool[0].subtree_max = 5;
    failed += expect("stale summary", &tree, "summary");
    tree = baseline(); tree.count = 4;
    failed += expect("wrong count", &tree, "count");
    tree = baseline(); tree.pool[1].left = 0;
    failed += expect("cycle", &tree, "repeated-node");
    tree = baseline(); tree.pool[0].right = CAPACITY;
    failed += expect("unknown child", &tree, "invalid-index");
    tree = (struct tree){.root=NIL,.first=NIL};
    failed += expect("empty", &tree, "ok");
    return failed ? 1 : 0;
}
