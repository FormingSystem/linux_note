#include <stdio.h>

struct model_device {
    const char *name;
    unsigned int kind;
    int present;
    int bound;
};

struct model_driver {
    unsigned int kind;
    int present;
};

static void try_bind(struct model_device *device, const struct model_driver *driver)
{
    if (!device->present || !driver->present || device->bound)
        return;
    if (device->kind != driver->kind)
        return;
    /* 模型只验证相遇，本行假定服务准备总能成功。 */
    device->bound = 1;
    printf("bound=%s\n", device->name);
}

static void register_device(struct model_device *device,
                            const struct model_driver *driver)
{
    device->present = 1;
    try_bind(device, driver);
}

static void register_driver(struct model_driver *driver,
                            struct model_device *devices, size_t count)
{
    size_t index;

    driver->present = 1;
    for (index = 0; index < count; ++index)
        try_bind(&devices[index], driver);
}

static int run_case(int driver_first)
{
    struct model_device devices[] = {{"A", 7, 0, 0}, {"B", 7, 0, 0}};
    struct model_driver driver = {7, 0};

    printf("case=%s\n", driver_first ? "driver_first" : "device_first");
    if (driver_first)
        register_driver(&driver, devices, 2);
    register_device(&devices[0], &driver);
    if (!driver_first)
        register_driver(&driver, devices, 2);
    register_device(&devices[1], &driver);
    /* 再次查找已经绑定的 A，不应再次打印绑定。 */
    try_bind(&devices[0], &driver);
    return devices[0].bound && devices[1].bound ? 0 : 1;
}

int main(void)
{
    int first = run_case(0);
    int second = run_case(1);

    if (first || second) {
        fputs("model check failed\n", stderr);
        return 1;
    }
    return 0;
}
