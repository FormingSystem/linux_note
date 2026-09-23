// SPDX-License-Identifier: MIT
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* 只观察定宽整数，不构造或解引用宿主指针。 */
static uint64_t width_mask(unsigned width)
{
    assert(width == 32 || width == 64);
    return width == 32 ? UINT32_MAX : UINT64_MAX;
}

static bool node_word(uint64_t base, unsigned type, unsigned width, uint64_t *out)
{
    if (base > width_mask(width) || (base & 255) || type > 3)
        return false;
    *out = base | ((uint64_t)type << 3) | 4;
    return true;
}

static bool parent_word(uint64_t base, unsigned slot, unsigned width, uint64_t *out)
{
    if (base > width_mask(width) || (base & 255) || slot > 31)
        return false;
    /* 仅覆盖固定实现中 range/arange 的父槽格式。 */
    *out = base | ((uint64_t)slot << 3) | 6;
    return true;
}

static bool root_parent_word(uint64_t tree, unsigned width, uint64_t *out)
{
    if (tree > width_mask(width) || (tree & 1))
        return false;
    *out = tree | 1;
    return true;
}

static bool reserved_entry(uint64_t entry)
{
    return entry < 4096 && (entry & 3) == 2;
}

static uint64_t error_word(int error, unsigned width)
{
    assert(error < 0);
    /* 先转无符号再移位，最后保留目标位宽，避免有符号负数左移。 */
    return (((uint64_t)(int64_t)error << 2) | 2) & width_mask(width);
}

int main(void)
{
    uint64_t enode, parent, root_parent;
    assert(node_word(0x1000,2,32,&enode));
    assert(parent_word(0x1000,17,32,&parent));
    assert(root_parent_word(0x1232,32,&root_parent));
    printf("enode=0x%" PRIx64 " type=%" PRIu64 "\n", enode, (enode >> 3) & 15);
    printf("parent=0x%" PRIx64 " slot=%" PRIu64 "\n", parent, (parent & 248) >> 3);
    printf("root parent=0x%" PRIx64 " tree=0x%" PRIx64 " wrong mask=0x%" PRIx64 "\n",
           root_parent, root_parent & ~UINT64_C(1), root_parent & ~UINT64_C(255));
    assert((enode & ~UINT64_C(255)) == 0x1000);
    assert((parent & ~UINT64_C(255)) == 0x1000);
    assert((root_parent & ~UINT64_C(1)) == 0x1232);
    assert(reserved_entry(6) && !reserved_entry(4098));
    assert(!node_word(0x1232,2,32,&enode));
    assert(!parent_word(0x1000,32,32,&parent));
    assert(!root_parent_word(0x1233,32,&root_parent));
    printf("error -12: word32=0x%" PRIx64 " word64=0x%" PRIx64 "\n",
           error_word(-12,32),error_word(-12,64));
    unsigned checks=0;
    for(unsigned width=32;width<=64;width+=32) {
        for(uint64_t base=0;base<65536;base+=256) {
            for(unsigned type=0;type<4;++type) {
                assert(node_word(base,type,width,&enode));
                assert((enode & ~UINT64_C(255))==base && ((enode>>3)&15)==type);
                ++checks;
            }
            for(unsigned slot=0;slot<32;++slot) {
                assert(parent_word(base,slot,width,&parent));
                assert((parent & ~UINT64_C(255))==base && ((parent&248)>>3)==slot);
                ++checks;
            }
        }
        uint64_t last_aligned=width_mask(width) & ~UINT64_C(255);
        assert(node_word(last_aligned,3,width,&enode));
        assert((enode & ~UINT64_C(255))==last_aligned);
    }
    printf("node/parent round trips: %u\n",checks);
    return 0;
}
