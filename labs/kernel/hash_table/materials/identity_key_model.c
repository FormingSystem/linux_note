/* 只验证完整身份的比较；数值标签模拟对象/命名空间身份，不模拟内核哈希或网络处理。 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct name_key {
    unsigned int parent_id;
    const char *component;
};
struct flow_key {
    unsigned int net_id, zone;
    uint32_t source, destination;
    uint16_t source_port, destination_port;
    uint8_t protocol;
};
struct neighbour_key {
    unsigned int device_id;
    uint32_t address;
};

static bool same_name(struct name_key a, struct name_key b)
{
    return a.parent_id == b.parent_id &&
           strcmp(a.component, b.component) == 0;
}
static bool same_flow(struct flow_key a, struct flow_key b)
{
    return a.net_id == b.net_id && a.zone == b.zone &&
           a.source == b.source && a.destination == b.destination &&
           a.source_port == b.source_port &&
           a.destination_port == b.destination_port &&
           a.protocol == b.protocol;
}
static bool same_neighbour(struct neighbour_key a, struct neighbour_key b)
{
    return a.device_id == b.device_id && a.address == b.address;
}

int main(void)
{
    struct name_key first_name = { 1, "app.conf" };
    struct name_key second_name = { 2, "app.conf" };
    assert(!same_name(first_name, second_name));
    second_name.parent_id = 1;
    assert(same_name(first_name, second_name));
    second_name.component = "other.conf";
    assert(!same_name(first_name, second_name));

    struct flow_key first_flow = { 1, 7, 10, 20, 3000, 443, 6 };
    struct flow_key second_flow = first_flow;
    assert(same_flow(first_flow, second_flow));
    second_flow.net_id = 2;
    assert(!same_flow(first_flow, second_flow));
    second_flow = first_flow;
    second_flow.zone = 8;
    assert(!same_flow(first_flow, second_flow));
    second_flow = first_flow;
    second_flow.source_port = 3001;
    assert(!same_flow(first_flow, second_flow));

    struct neighbour_key first_neighbour = { 1, 10 };
    struct neighbour_key second_neighbour = { 2, 10 };
    assert(!same_neighbour(first_neighbour, second_neighbour));
    second_neighbour.device_id = 1;
    assert(same_neighbour(first_neighbour, second_neighbour));
    puts("九项身份比较通过；键相等不等于权限、协议状态或链路可用");
    return 0;
}
