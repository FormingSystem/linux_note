#include <stdbool.h>
#include <stdio.h>

enum endpoint_direction {
    ENDPOINT_RECEIVE,
    ENDPOINT_TRANSMIT
};

struct endpoint_identity {
    unsigned int channel;
    enum endpoint_direction direction;
};

static bool decode_minor(unsigned int minor, struct endpoint_identity *identity)
{
    /* 先检查范围，再做减法；不能让空洞变成数组下标。 */
    if (minor < 4) {
        identity->channel = minor;
        identity->direction = ENDPOINT_RECEIVE;
        return true;
    }
    if (minor >= 16 && minor < 20) {
        identity->channel = minor - 16;
        identity->direction = ENDPOINT_TRANSMIT;
        return true;
    }
    return false;
}

int main(void)
{
    const unsigned int minors[] = {0, 3, 4, 15, 16, 19, 20};
    struct endpoint_identity identity;

    for (size_t index = 0; index < sizeof(minors) / sizeof(minors[0]); ++index) {
        if (!decode_minor(minors[index], &identity)) {
            printf("minor=%u invalid\n", minors[index]);
            continue;
        }
        printf("minor=%u channel=%u direction=%s\n", minors[index],
               identity.channel,
               identity.direction == ENDPOINT_RECEIVE ? "receive" : "transmit");
    }
    return 0;
}
