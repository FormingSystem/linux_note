typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

struct packed_record {
    u8 tag;
    u64 value;
} __attribute__((packed));

/* 自然对齐的 32 位读取。 */
u32 load_aligned_u32(const volatile u32 *value)
{
    return *value;
}

/* 自然对齐的 64 位读取。 */
u64 load_aligned_u64(const volatile u64 *value)
{
    return *value;
}

/* value 位于偏移 1，编译器不能假定自然对齐。 */
u64 load_packed_u64(const volatile struct packed_record *record)
{
    return record->value;
}

/* 未对齐 64 位写入也可能被拆分。 */
void store_packed_u64(volatile struct packed_record *record, u64 value)
{
    record->value = value;
}
