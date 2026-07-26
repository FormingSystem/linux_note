---
id: tools.practice_tool.security_version_update_and_lifecycle
title: "训练工具安全、版本更新与软件生命周期设计"
kind: design
status: evolving
domains:
  - tools
  - security
---

# 第1章\_训练工具安全、版本更新与软件生命周期设计

## 1.1\_受保护的发布信息

`config/release.json` 和 `config/security.json` 是随版本发布、由 Git 管理的只读清单。浏览器可以查询并展示这些信息，但服务端不提供修改接口。它们不能和题库、答案等允许在线编辑的业务内容共用保存接口。

本设计不依赖 Windows 文件只读属性或 Unix 权限位表达发布安全边界，因为该属性不能稳定地随 Git 跨平台传播。发布信息的修改必须进入源码变更、校验和 Git 审查流程。

本地服务只监听 `127.0.0.1`。更新写操作只接受启动时随机生成的会话令牌，并且只允许执行预先定义的 Git 参数，不接受浏览器提交任意命令。

## 1.2\_非打扰更新

开发服务启动后延迟检查远端分支，此后按发布清单中的周期后台检查。没有更新时界面不弹窗、不抢占输入焦点；发现更新时只显示一条状态栏。

用户点击更新后，服务只执行快进更新。仓库存在未提交或未跟踪修改时拒绝自动更新，防止覆盖用户工作。完成后提示重新启动工具，不在当前进程中热替换程序代码。

## 1.3\_软件登记与所有权

本机软件事实登记在 `.local/software_registry.tsv`。该文件属于本机状态，不进入 Git。登记表只保存固定字段，不保存或执行任意卸载命令。

| 所有权 | 含义 | 更新规则 | 干净卸载 |
|---|---|---|---|
| `tool-owned` | 安装前不存在，由本工具从无到有安装 | 可以由工具维护 | 删除 |
| `external` | 安装前已经存在，工具继续使用原版本 | 兼容时直接使用 | 永不删除 |
| `external-updated` | 安装前已经存在，用户明确同意由工具更新 | 更新前必须询问 | 永不删除 |

一次更新不会改变软件的原始所有权。外部软件被本工具更新后仍是用户或系统所有，不能因为工具参与过更新就进入干净卸载清单。非交互运行时不自动更新外部软件。

`cleanup_kind` 只允许 `local-runtime`、`download-cache`、`msys-package`、`msys-root` 和 `none`。卸载器还会核对固定目录或预期包名，不能把登记表中的路径当成任意删除目标。

## 1.4\_两级卸载

最小卸载删除 `node_modules`、`dist`、环境就绪标记、日志以及当前用户的补全登记。它保留 `.local/runtime`、`.local/downloads` 和软件登记表，以便重新安装依赖后快速恢复。

干净卸载先执行最小卸载，再依据登记表删除 `tool-owned` 的隔离 Node.js、下载缓存和由工具从无到有安装的 MSYS2 包。`external` 与 `external-updated` 无条件跳过。

若 MSYS2 本身由 PowerShell 冷启动脚本安装，正在运行的 MSYS2 Bash 不能删除承载自己的根目录。干净卸载会保留登记表并提示退出 Bash 后执行 `uninstall_windows.ps1 -Clean`；该脚本再次核对 `tool-owned` 与固定根目录形态后才调用官方卸载器。

卸载不会删除 Git 管理的源码、题库、配置和文档，也不会擅自清除浏览器中的训练答案。浏览器数据应由用户在界面的数据管理功能中单独导出或删除。

## 1.5\_命令入口

```bash
./install.sh
./run.sh
./uninstall.sh --minimal
./uninstall.sh --clean

# 也可从统一启动入口转发
./start.sh --uninstall minimal
./start.sh --uninstall clean
```

安装、运行和卸载是互相独立的生命周期模块。`run.sh` 不得调用安装逻辑；`start.sh` 只在首次运行或环境缺失时使用 `install.sh --if-needed`，然后转交 `run.sh`。
