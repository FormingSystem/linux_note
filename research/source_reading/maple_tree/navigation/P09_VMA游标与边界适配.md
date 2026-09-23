---
id: research.maple_tree.navigation.vma_adapter
title: "VMA游标与边界适配"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第9章\_VMA游标与边界适配

## 9.1\_同一树可以有多个局部游标

沿[总索引](P01_Linux_6.12_Maple范围源码阅读索引.md#1.1_从地址查询进入版本证据)进入固定 Linux 6.12.20。mm_struct 保存地址空间共享的 mm_mt；调用方的 vma_iterator 内嵌 ma_state 关联这棵树，局部位置不属于 VMA 本身。线程共享 mm 时也共享索引。教材见[P15 接入层](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.10_VMA_接入层_mm_struct.mm_mt)。

## 9.2\_从地址空间到局部游标

| 阶段 | 状态与参与者 | 固定实现入口 |
| --- | --- | --- |
| S0 外围保护 | 调用方建立 mmap/VMA 协议；树的外部锁登记不替代取得锁 | [树模式模块](P03_树对象与模式选择.md#3.2_从未发布到受保护使用) |
| S1 初始化 | VMA_ITERATOR 宏 last=0，vma_iter_init 经 mas_init 设置 last=addr；共同建立 start 入口 | [两种初始化](../source_explanations/include/linux/mm_types.h.md#1.3_VMA游标与两种初始化) |
| S2 查询 | vma_find 转 max-1，vma_next 首次也使用 find，prev 与 next_range 选择不同方向/范围任务 | [窗口](../source_explanations/include/linux/mm.h.md#1.4_VMA查找复用高级游标)、[方向](../source_explanations/include/linux/mm.h.md#1.5_VMA方向与范围遍历) |
| S3 使用对象 | 调用方在对象稳定协议内使用返回 VMA，游标不额外取得引用 | [范围查询模块](P02_范围契约与查询入口.md#2.4_返回指针的使用期限) |
| S4 结束或暂停 | invalidate 调用 pause；set 改起点；free 释放本次状态资源 | [暂停](../source_explanations/include/linux/mm.h.md#1.3_VMA游标失效调用暂停)、[状态资源退出](../source_explanations/include/linux/mm.h.md#1.6_VMA写入请求与资源退出) |

## 9.3\_转发与边界检查各有责任

bulk_store 将 VMA 的 start/end-1 写入状态并调用 mas_store；clear_gfp 以 __mas_set_range 设 start/end-1 后写 NULL。二者按状态错误映射为 -ENOMEM，不能把全部 errno 原样转发或以旧 entry 为 NULL 推断成功。唯一实现见[写入包装](../source_explanations/include/linux/mm.h.md#1.6_VMA写入请求与资源退出)。

这些函数依赖合法范围、既有状态与外围保护，不能把 end=0 的减一当作空窗口。P15 的 C 整数模型主动拒绝空/逆序输入，528 组有效区间核对 17424 次成员关系，561 组无效输入验证输出不变；它不是内核封装自动检查输入的证据。

本批固定包装的宿主夹具只核对初始化字段、转发参数及错误映射，Maple 查询/写入是显式替身。没有执行真实 VMA 更新、锁竞争和页表操作。固定 mm/mmap.c 另有 mm/vma.h 的 config/prealloc/store 调用链，后续应沿真实调用点展开，不能将此处的公共头文件包装表冒充完整 MM 写入路径。
