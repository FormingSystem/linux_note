# Typora C 与 Mermaid 增强验收

```c
#include <linux/rcupdate.h>

struct demo_cfg {
    int generation;
    struct rcu_head rcu;
};

static struct demo_cfg __rcu *current_cfg;

static void publish_cfg(struct demo_cfg *new_cfg)
{
    struct demo_cfg *old_cfg;

    old_cfg = rcu_replace_pointer(current_cfg, new_cfg, true);
    if (old_cfg != NULL)
        call_rcu(&old_cfg->rcu, demo_cfg_free_rcu);
}

static int read_generation(void)
{
    const struct demo_cfg *cfg;
    int generation = -1;

    rcu_read_lock();
    cfg = rcu_dereference(current_cfg);
    if (cfg != NULL)
        generation = cfg->generation;
    rcu_read_unlock();

    return generation;
}

static void replace_generation(int generation)
{
    struct demo_cfg *new_cfg;

    new_cfg = kzalloc(sizeof(*new_cfg), GFP_KERNEL);
    if (new_cfg == NULL)
        return;

    new_cfg->generation = generation;
    publish_cfg(new_cfg);
}

static void clear_generation(void)
{
    struct demo_cfg *old_cfg;

    old_cfg = rcu_replace_pointer(current_cfg, NULL, true);
    if (old_cfg != NULL)
        call_rcu(&old_cfg->rcu, demo_cfg_free_rcu);
}
```

```mermaid
flowchart LR
    A[reader 进入读侧] --> B[读取 current_cfg]
    B --> C{writer 是否发布新对象}
    C -- 否 --> D[继续读取旧对象]
    C -- 是 --> E[新 reader 取得 new_cfg]
    D --> F[reader 退出]
    E --> G[旧对象等待宽限期]
    F --> H[报告静止状态]
    H --> I[宽限期完成]
    G --> I
    I --> J[调用回调释放 old_cfg]
```
