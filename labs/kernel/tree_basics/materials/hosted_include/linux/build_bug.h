/* 宿主实验适配层：只提供固定 container_of.h 所需的两个编译期设施。 */
#ifndef TREE_DEMO_BUILD_BUG_H
#define TREE_DEMO_BUILD_BUG_H
#define static_assert _Static_assert
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
#endif
