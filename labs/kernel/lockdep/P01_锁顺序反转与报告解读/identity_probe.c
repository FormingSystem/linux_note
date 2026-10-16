#include <stdio.h>

struct identity_probe {
	const int *key;
};

static void init_config_identity(struct identity_probe *probe)
{
	/* 模拟一个初始化调用点的静态身份，生命周期覆盖整个程序。 */
	static int config_key;
	probe->key = &config_key;
}

static void init_state_identity(struct identity_probe *probe)
{
	/* 第二个调用点有另一个静态对象，不因值相同而合并身份。 */
	static int state_key;
	probe->key = &state_key;
}

int main(void)
{
	struct identity_probe config_a, config_b, state_a;

	init_config_identity(&config_a);
	init_config_identity(&config_b);
	init_state_identity(&state_a);
	printf("different_instances=%d\n", &config_a != &config_b);
	printf("same_call_site_identity=%d\n", config_a.key == config_b.key);
	printf("different_call_site_identity=%d\n", config_a.key != state_a.key);
	return 0;
}
