#if !__has_attribute(btf_type_tag)
#error 当前编译器不支持本实验属性
#endif

struct tag_sample {
    int __attribute__((btf_type_tag("user"))) *source;
};

struct tag_sample sample;

unsigned int tagged_pointer_size(void)
{
    /* 只观察类型大小，不解引用source。 */
    return sizeof(sample.source);
}
