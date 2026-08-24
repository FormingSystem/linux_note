---
id: labs.kernel.memory_ordering.lkmm_litmus_message_passing
title: "LKMM Litmus 消息传递与屏障实验"
kind: lab
status: evolving
domains:
  - linux
  - kernel
  - engineering
topics:
  - memory_ordering
  - lkmm
  - formal_methods
---

# 第1章\_LKMM\_Litmus\_消息传递与屏障实验

## 1.1\_实验目标

使用 Linux 6.12.20 保存的 LKMM 和 herd7，对同一并发模式运行无序/有序成对测试：

1. MP：只有 ONCE 时坏结果允许，release/acquire 或 wmb/rmb 后禁止；
2. SB：只有 ONCE 时 0/0 允许，两边 full barrier 后禁止；
3. IRIW：reader 无屏障时相反观察允许，两次读取间 full barrier 后禁止；
4. RCU 指针：`rcu_assign_pointer()` / `rcu_dereference()` 禁止取得新指针却看到预初始化旧值。

理论前置：[LKMM 事件、关系与一致性判定](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/P08_LKMM事件_关系与一致性判定.md)和 [Litmus 验证方法](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/P09_Litmus_形式验证与硬件实验.md)。

## 1.2\_证据来源

模型来自：

```text
research/source_reading/linux/tools/memory-model/
```

测试基于 Linux 6.12.20 `tools/memory-model/litmus-tests/` 中同名代表性测试保存。`manifest.json` 固定预期 Observation，`run.py` 会核对输出，不以人工目测代替批量判定。

## 1.3\_环境

- Python 3.10 或更新版本；
- herdtools7 提供的 `herd7`，Linux 6.12 模型 README 要求 7.52 或更新版本；
- 不需要构建整个 Linux 内核；
- 如要用 klitmus7 转成内核模块，必须另备匹配目标内核和可恢复测试环境。

当前 Windows 编辑环境没有安装 herd7，因此本次仓库验证只执行 `--check` 检查文件、模型和清单，不伪造模型运行结果。正式实验应在具备 herdtools7 的 Linux/WSL 环境执行并保存输出。

## 1.4\_静态检查

```bash
cd labs/kernel/memory_ordering/P02_LKMM_Litmus_消息传递与屏障
python run.py --check
```

该命令验证：

- 模型五个核心文件存在；
- manifest 中每个测试文件存在；
- 预期结果只使用 `Sometimes`/`Never`；
- 是否能在 PATH 中找到 herd7。

缺少 herd7 时静态检查仍成功，但明确打印“只能检查，不能运行”。

## 1.5\_运行全部测试

```bash
python run.py
```

输出保存到 `generated/<test>.txt`。脚本从每份输出解析 `Observation`，与 manifest 比较；任何不一致都会返回失败。

只运行名称包含 MP 的测试：

```bash
python run.py --filter MP
```

## 1.6\_预期结果矩阵

| 测试 | 关注坏结果 | 预期 |
| --- | --- | --- |
| `MP+poonceonces` | flag=1 且 data=0 | Sometimes |
| `MP+pooncerelease+poacquireonce` | 同上 | Never |
| `MP+fencewmbonceonce+fencermbonceonce` | 同上 | Never |
| `SB+poonceonces` | 两边都读 0 | Sometimes |
| `SB+fencembonceonces` | 两边都读 0 | Never |
| `IRIW+poonceonces+OnceOnce` | 两 reader 对写顺序意见相反 | Sometimes |
| `IRIW+fencembonceonces+OnceOnce` | 同上 | Never |
| `MP+onceassign+derefonce` | 取得新 RCU 指针却读到旧载荷 | Never |

## 1.7\_逐项解释要求

每次实验记录不能只贴 `Sometimes/Never`，还要回答：

1. 每个寄存器从哪个 Write 取值；
2. 无序版本缺哪条边；
3. 有序版本新增 release/acquire、rmb/wmb 或 mb 中哪条边；
4. 为什么该边让关注结果形成模型禁止关系；
5. 该测试没有覆盖哪些真实代码责任，例如多写者、代际和对象生命期。

## 1.8\_失败现象与排查

| 现象 | 排查 |
| --- | --- |
| 找不到 herd7 | 安装 herdtools7 并确认 PATH；先运行 `herd7 -version` |
| 模型 include 失败 | 脚本必须以模型目录为 cwd，勿直接改 cfg 相对路径 |
| Observation 无法解析 | 保存完整输出，核对 herd7 版本和测试名 |
| 结果与 manifest 不符 | 核对模型/测试是否来自同一 Linux 版本，再检查工具兼容性 |
| `Sometimes` 的硬件测试不出现 | 模型允许不代表有限运行必现 |

## 1.9\_实际结果记录

正式运行后，将以下内容保存到独立结果文档：herd7 版本、Linux 模型基线、完整命令、每个 generated 输出的 hash、结果矩阵和逐项事件解释。若执行 klitmus7，还需记录目标 CPU、内核版本/配置、迭代数和各结果次数。

## 1.10\_清理

```bash
python run.py --clean
```

只删除本实验目录内固定的 `generated/`。若另外生成和加载了 klitmus7 内核模块，必须先按生成脚本卸载模块，再删除生成目录。
