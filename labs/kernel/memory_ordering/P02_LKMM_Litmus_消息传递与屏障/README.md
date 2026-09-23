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

使用Linux 6.12.20保存的LKMM和herd7，对同一并发模式运行无序/有序成对测试。我们询问一个坏结果是否允许；以下是清单中的预期，只有实际执行工具后才能填写本次结果：

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

测试基于Linux 6.12.20的代表性Litmus保存。[manifest.tsv](manifest.tsv)是唯一批量清单：头部保存模型版本，之后每行一个文件名和预期分类，用制表符分隔。[run.sh](run.sh)负责调用外部工具并核对实际Observation；它使用Bash编排命令，不需要Python，也不自行实现内存模型。模型来自[NXP官方固定提交](../../../../research/source_reading/linux/SOURCE_BASELINE.md#1.1_当前来源)，不能用本地实验HEAD代替来源身份。

## 1.3\_环境

- Bash 4或更新版本及常见coreutils（包含sha256sum、realpath）、Git；Linux或MSYS2 Bash可执行静态检查和驱动；
- herdtools7提供的 `herd7`，固定模型README提出7.52或更新版本，同时提醒未来兼容性并非绝对保证；
- 不需要构建整个 Linux 内核；
- 如要用 klitmus7 转成内核模块，必须另备匹配目标内核和可恢复测试环境。

2026-09-24本次宿主PATH没有herd7，实际模型未执行。正式实验应在具备匹配herdtools7的环境运行并保存输出；可以用 `--herd /path/to/herd7` 指定工具。klitmus7另有目标内核兼容表：固定README对5.17及之后内核列出7.56.1或以上，不能将herd模型的最低版本直接当成6.12硬件模块生成器的充分条件。

## 1.4\_静态检查

```bash
cd labs/kernel/memory_ordering/P02_LKMM_Litmus_消息传递与屏障
bash run.sh --check
```

该命令验证：

- 模型五个核心文件存在；
- 清单头部和每个测试文件存在，文件声明的测试名与文件名一致；
- 文件名没有路径穿越、没有重复项，预期只使用Sometimes/Never/Always；
- 筛选至少匹配一个测试；
- 能否在PATH中找到herd7，找到时查询版本，但不执行模型。

缺少herd7时静态检查仍成功，明确打印“未执行模型”；这只说明实验材料通过结构检查。五个文件存在也不能保证herd7的内部库或包含关系一定兼容，后者必须实际解析模型。

## 1.5\_运行全部测试

```bash
bash run.sh
```

脚本以模型目录为工作目录，向herd7传入输入的绝对路径，避免从不同位置启动时解析错cfg引用。工具缺失返回2；运行非零退出、Observation缺失/重复、测试名不匹配、分类或计数异常、与预期不符都会失败。不会从失败进程的半截输出里捡一行Never算通过。

输出分工如下：

| 文件 | 复查时回答的问题 |
| --- | --- |
| `generated/run.txt` | 实际命令、仓库HEAD、模型目录和工具路径是什么，各项退出/分类怎样，全部所选项是否通过 |
| `generated/herd_version.txt` | 哪个工具版本解释了模型，版本查询本身是否报错 |
| `generated/inputs.sha256` | 清单、五个核心模型文件和八份输入内容是否与这次一致 |
| `generated/<test>.stdout.txt` | 状态列表、见证与Observation实际是什么 |
| `generated/<test>.stderr.txt` | 解析、配置或工具执行报告了什么错误 |

仓库HEAD不是未提交工作文件的内容指纹，因此还需要输入摘要。脚本成功也不意味着其他工具警告都可忽略，应读取完整输出及诊断；预期结果表不是证据文件。

只运行名称包含 MP 的测试：

```bash
bash run.sh --filter MP
```

筛选不区分大小写，MP当前匹配四项，包括RCU指针测试。下一次实际执行前会清除脚本已知的旧输出，防止未执行项留下旧成功结果；若静态检查或工具发现阶段提前失败，本轮日志尚未开始，不能用旧run.txt覆盖终端失败结论。长期证据先独立归档，不能只依赖会重建的generated目录。`--check` 不创建或清理这些结果。

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

先看run.txt中的进程退出状态，再看stderr，最后才判断Observation。若工具进程失败，修复环境或输入后重跑；若工具成功却与预期不同，保留两份输入与见证图，按P08检查读取来源、屏障和模型版本，不要直接修改清单期望来让检查变绿。

## 1.9\_实际结果记录

正式运行后，将以下内容保存到独立结果文档：herd7 版本、Linux 模型基线、完整命令、每个 generated 输出的 hash、结果矩阵和逐项事件解释。若执行 klitmus7，还需记录目标 CPU、内核版本/配置、迭代数和各结果次数。

本批已经执行的是：八项输入静态检查，以及隔离目录中16项Bash驱动协议检查，覆盖全部八项解析、MP筛选、空筛选、工具缺失、工具版本失败、进程失败时两路输出保留、缺失/重复/错名Observation、计数矛盾、零允许执行、分类不符、重复清单和清理保留手工文件。工具替身明确输出“no LKMM execution”，这些检查验证实验装置，没有产生真实模型Observation；未运行herd7或klitmus7，不填写伪造的结果矩阵。

## 1.10\_清理

```bash
bash run.sh --clean
```

只删除本实验generated目录内按清单计算的已知输出和三份记录，不递归删除目录；其余手工文件、子目录以及独立klitmus产物保留。链接形式的generated目录会被拒绝。若修改清单后存在旧名结果，应先检查并人工归档，不扩大清理范围。若另外生成和加载了klitmus7内核模块，必须先核对实际模块并卸载；删除文件不会终止内核中的代码。
