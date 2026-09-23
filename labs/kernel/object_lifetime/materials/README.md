---
id: labs.kernel.object_lifetime.materials
title: "对象生命周期实验材料"
kind: reference
status: evolving
domains:
  - linux
  - kernel
  - object_lifetime
---

# 第1章\_对象生命周期实验材料

## 1.1\_引用责任与交付

[reference_ownership.c](reference_ownership.c)对应[kref 问题入口的完整程序](../../../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.6.1_运行完整的责任交接模型)。在宿主目录用 `cc -std=c11 -Wall -Wextra -Werror -O2 reference_ownership.c -o reference_ownership` 编译并运行，保留断言。输出为 `accept=0 released=1` 和 `accept=1 released=2`，表示拒绝与接收两条路径分别完成一次最终回收。

程序使用普通 C 分配对象和责任槽，按顺序安排动作，不是 Linux `kref` 的替代实现。字段借用、创建者与处理者两个结束顺序、直接转交和拒绝归还的推导见正文，不在此复制另一份教程。

本轮宿主检查包含六种所有权调度组合、两条受控分配失败路径和槽责任求和。它们未验证内核模块、真实 workqueue、并发访问、原子计数、内存序、饱和告警或 KASAN；没有主动执行 UAF。

## 1.2\_一次内核工作交付

[note_kref_work.c](note_kref_work.c)与[Makefile](Makefile)对应[P01 完整模块](../../../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.16.1_运行一次真实工作交付)。它把 ref 放在业务值之后的非首成员位置，沿[P02 地址关系](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.9_container_of_是理解_kref_的关键)检查两种回调还原；只投递一个新工作项，成功后创建者和 worker 各归还一份；卸载等待私有队列销毁，回调不重排。目标构建和观察步骤在正文；本轮未实际装卸目标模块。

ARM Clang 前端检查通过，纳入的 354 份头文件中非生成源码与官方固定提交无差异，生成头仍属于当前配置环境。宿主 C 控制夹具采用固定 kref_init/get/put 函数体，把底层计数、分配和队列明确替换为顺序模型，覆盖队列分配失败、对象分配失败、投递拒绝、worker 先结束、创建者先结束五条路径。此检查不等于目标构建链接、原子并发或 workqueue 实际运行。

## 1.3\_回绕与饱和的保守代价

[count_wrap.c](count_wrap.c)配合[P02 八位模型](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.5.1_用八位模型观察回绕的代价)，以 uint8_t 普通回绕和显式布尔饱和状态比较同一组责任事件。严格 C11 编译运行及 1001 组正常/饱和责任序列通过。该模型不分配/释放真实对象，不是 Linux refcount_t 的算法、位宽、并发或告警实现；不能拿模型结果证明内核竞态安全。

B04d 布局调整后重新通过 ARM 前端、354 份头文件固定源码差异检查和五条控制路径，宿主夹具另断言 ref 非首成员且 work 位于其后。复用的树专题 embedded_owner 程序重新运行通过；这些验证没有执行目标模块或检测真实并发。

## 1.4\_静态存储中的归零

[note_kref_static.c](note_kref_static.c)配合[P02 静态模块](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.14.2_运行一个不释放静态内存的完整模块)观察 1→2→1→0。计数归零同步调用清理回调，静态外壳由模块存储机制回收，回调不 kfree 它。构建、装卸命令和练习均在正文。

本轮 ARM 前端通过，348 份头文件中的 336 份非生成源码与固定提交无差异；宿主用固定类型、初始化宏、引用函数与顺序原子替身检查一次归零回调。自动运行时初始化与静态常量初始化通过严格 C11 编译；缺少内层花括号得到预期 missing-braces 诊断，裸宏赋值和非恒定静态初始化按预期编译失败。未执行目标构建链接、模块装卸或真实并发；宿主控制结果不能冒充目标日志。

## 1.5\_快照与持有的区别

[reference_snapshot.c](reference_snapshot.c)对应[P02 对照程序](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.17.1_运行快照与持有的对照程序)。第一轮只保留正计数快照，原持有者归还后对象回收；第二轮先追加观察者责任，原持有者退出后仍可访问，再由观察者完成最终归还。程序只打印对象外的回收记录，不故意访问悬空指针。

严格 C11 编译与两条顺序路径、两条受控分配失败检查通过；另以 C 枚举“减后另读”的六种保序交错，其中四种出现两个清理资格，与绑定本次减少旧值的两种顺序恰有一个资格对照。教学计数为普通整数，交错由测试顺序指定，不证明真实并发、内存序或目标内核行为。

## 1.6\_类型接口与部分初始化清理

[note_kref_object.c](note_kref_object.c)是[P02 完整对象模板](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.19_标准自定义引用对象模板)的配套模块。外壳与 data 分别申请；init 建立创建者份额，部分初始化失败和正常最后归还共用 release，先清理 data 再清理外壳。NULL put 是本类型明确允许的空槽操作，get 仍须非空且已有正引用保护。

ARM 前端通过，354 份头文件中 342 份非生成源码与固定提交无差异。宿主 C 夹具采用实际模块代码和固定普通引用函数、明确的顺序原子/分配替身，覆盖成功、外壳分配失败、data 申请失败及空槽 put，检查释放顺序、回调次数、存活块归零和 ref 非首成员。未执行目标链接、装卸、真实分配器故障注入或并发。

## 1.7\_与C++管理型指针对照

[shared_ownership.cpp](shared_ownership.cpp)对应[P02 C++ 对照](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.23.1_用完整程序观察自动归还)，只使用 C++17 接口。严格编译运行观察到拷贝后两份、移动后原管理型对象为空、reset 后一份、异常退出后析构一次。get 得到的裸指针没有独立份额，全部计数断言限定在该顺序程序。

另用既有固定 kref 普通函数和顺序原子替身检查两种归还顺序，均只调用最后一次 put 传入的回调；把 kfree 的 const void * 签名直接传给 kref_put 的负例按预期触发函数指针不兼容诊断。未执行错误回调、悬空访问或真实并发。
