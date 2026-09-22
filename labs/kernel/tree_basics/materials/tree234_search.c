#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* 只读教学模型：关键字唯一，非叶节点恰有 key_count + 1 个孩子。 */
struct tree234_node {
    unsigned int key_count;
    int keys[3];
    const struct tree234_node *children[4];
};

struct search_result {
    const struct tree234_node *node;
    unsigned int index;
    unsigned int visited_nodes;
    unsigned int compared_keys;
};

static struct search_result search_tree(const struct tree234_node *node, int key)
{
    struct search_result result = {0};
    while (node) {
        unsigned int index = 0;
        ++result.visited_nodes;
        /* 计数的是参与本轮决策的关键字，不是机器指令或耗时。 */
        while (index < node->key_count) {
            ++result.compared_keys;
            if (key <= node->keys[index])
                break;
            ++index;
        }
        if (index < node->key_count && key == node->keys[index]) {
            result.node = node;
            result.index = index;
            return result;
        }
        /* 合法叶子的所有孩子均为 NULL；未命中时自然结束。 */
        node = node->children[index];
    }
    return result;
}

static const struct tree234_node leaf_a = {3, {5, 10, 15}, {NULL}};
static const struct tree234_node leaf_b = {3, {25, 30, 35}, {NULL}};
static const struct tree234_node leaf_c = {3, {45, 50, 55}, {NULL}};
static const struct tree234_node leaf_d = {3, {65, 70, 75}, {NULL}};
static const struct tree234_node branch_l = {1, {20}, {&leaf_a, &leaf_b}};
static const struct tree234_node branch_r = {1, {60}, {&leaf_c, &leaf_d}};
static const struct tree234_node root = {1, {40}, {&branch_l, &branch_r}};

/* 与树拓扑独立的线性集合，仅供检查返回答案。 */
static bool expected_contains(int key)
{
    static const int expected[] = {
        5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75
    };
    size_t i;
    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
        if (expected[i] == key)
            return true;
    return false;
}

int main(void)
{
    static const int queries[] = {55, 52, 40, 80};
    size_t i;
    int key;
    struct search_result empty = search_tree(NULL, 40);
    assert(!empty.node && empty.visited_nodes == 0 && empty.compared_keys == 0);
    for (i = 0; i < sizeof(queries) / sizeof(queries[0]); ++i) {
        struct search_result result = search_tree(&root, queries[i]);
        printf("key=%d found=%d nodes=%u compared_keys=%u\n",
               queries[i], result.node != NULL,
               result.visited_nodes, result.compared_keys);
    }
    for (key = 0; key <= 80; ++key) {
        struct search_result result = search_tree(&root, key);
        assert((result.node != NULL) == expected_contains(key));
        if (result.node)
            assert(result.index < result.node->key_count &&
                   result.node->keys[result.index] == key);
        assert(result.visited_nodes >= 1 && result.visited_nodes <= 3);
    }
    puts("range_checks=81 passed");
    return 0;
}
