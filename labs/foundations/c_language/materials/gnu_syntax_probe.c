#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define DEMO_MIN(x, y) ({                         \
    typeof(x) demo_min_x = (x);                   \
    typeof(y) demo_min_y = (y);                   \
    _Static_assert(__builtin_types_compatible_p(  \
        typeof(demo_min_x), typeof(demo_min_y)),  \
        "DEMO_MIN requires matching types");     \
    demo_min_x < demo_min_y ? demo_min_x : demo_min_y; \
})

struct natural_record { uint8_t tag; uint32_t sequence; };
struct packed_record {
    uint8_t tag;
    uint32_t sequence;
} __attribute__((packed));

/* 只模拟USB描述符前缀的宽度，不声明这是完整USB结构。 */
struct usb_prefix_demo {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t usb_version;
    uint8_t device_class;
} __attribute__((packed));

struct aligned_counter {
    unsigned long count;
} __attribute__((aligned(64)));

int main(void)
{
    int a = 10;
    int result = DEMO_MIN(a++, 20);

    printf("natural: offset=%zu size=%zu\n",
           offsetof(struct natural_record, sequence),
           sizeof(struct natural_record));
    printf("packed: offset=%zu size=%zu\n",
           offsetof(struct packed_record, sequence),
           sizeof(struct packed_record));
    printf("usb prefix: version_offset=%zu size=%zu\n",
           offsetof(struct usb_prefix_demo, usb_version),
           sizeof(struct usb_prefix_demo));
    printf("aligned: alignment=%zu size=%zu\n",
           _Alignof(struct aligned_counter), sizeof(struct aligned_counter));
    printf("minimum=%d next_a=%d\n", result, a);

    if (result != 10 || a != 11 ||
        offsetof(struct packed_record, sequence) != 1 ||
        sizeof(struct packed_record) != 5 ||
        offsetof(struct usb_prefix_demo, usb_version) != 2 ||
        sizeof(struct usb_prefix_demo) != 5 ||
        _Alignof(struct aligned_counter) < 64)
        return 1; /* 检查与执行都不放在可被关闭的断言中。 */
    return 0;
}
