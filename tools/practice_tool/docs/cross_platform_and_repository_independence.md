---
id: tools.practice_tool.cross_platform_and_repository_independence
title: "跨平台与仓库独立性设计"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_跨平台与仓库独立性设计

## 1.1\_目标

回路是可独立安装、独立发布、独立测试的 Electron Markdown 工作台。用户通过系统对话框打开任意单个 Markdown 或单个文件夹；`linux-note` 只是可能被打开的一个普通目录，不是运行时依赖。

最终用户不需要浏览器、Vite、本地 HTTP、Node.js、npm、CMake、C++ 编译器、MSYS2、Bash、Rust 或数据库运行时。Vite、Node.js、CMake、C++ 编译器、打包脚本和测试工具只属于源码开发环境。

## 1.2\_首发平台

第一阶段正式支持：

- Windows 10/11 x64。
- Ubuntu 22.04 x64。

两个平台使用同一产品语义、IPC Schema、Markdown fixture 和设置 Schema。平台差异只能进入 Main 的文件系统、系统对话框、回收站、窗口、安装包和更新适配器。

macOS、ARM64、便携版、多根工作区和商店分发不在第一阶段承诺中；加入前分别评审签名、公证、路径、权限与更新边界。

## 1.3\_最终用户运行时

- Electron 携带匹配版本的 Chromium 与 Node 运行时。
- 正式窗口加载打包后的 `loop-app://` 资源。
- 文件访问只经过 Electron Main，不监听本地端口。
- 安装包不得在首次启动时下载开发运行时或执行 npm/pacman。
- 应用可以在没有 Git、没有网络、没有仓库配置的普通目录中工作。

## 1.4\_独立仓库目标

目标仓库只包含：

```text
apps/desktop/
packages/
tests/
docs/
build and release configuration
package manifests and lockfile
```

不携带 `linux-note` 的 `knowledge/`、`practice.sources.json`、根快捷脚本、治理规则、训练 `banks/` 或电子书 Schema。打开目录时只消费标准文件系统和 Markdown，不要求目录安装回路专属文件。

## 1.5\_开发模式与正式模式

开发模式允许 Vite 热更新 Renderer，但仍要求：

- Main、Preload、Renderer 和 Worker 使用正式进程边界。
- Renderer 保持 sandbox、context isolation 和禁用 Node integration。
- 文件 API 仍走正式 IPC 与窗口能力表。
- 不因开发便利开放任意绝对路径、关闭 webSecurity 或加载远程生产 UI。

正式包不包含开发服务器入口。E2E 必须同时覆盖开发构建和安装后生产包，安全与性能结论只以生产包为准。

## 1.6\_跨平台文件语义

公共 `FileService` 定义文档读取、保存、监听、移动与删除语义。业务实现优先使用 C++ 标准库；真实路径、文件身份、链接、加密散列和安全随机数等标准库不足的能力，使用固定版本的跨平台库接口补齐。Windows 与 Ubuntu 仍有不同的可观察文件系统语义：

| 能力 | Windows | Ubuntu |
| --- | --- | --- |
| 路径 | 盘符、UNC、长路径、大小写 | POSIX、大小写、mount |
| 链接 | junction、reparse point、symlink | symlink、bind mount |
| 保存 | 占用、替换 API、ACL | rename、mode、fsync 目录 |
| 文件身份 | volume/file ID | device/inode |
| 删除 | 系统回收站 | 桌面 trash/portal |
| 监听 | 平台 watcher 归一化 | inotify 资源限制与降级 |

公共层不拼接平台路径，不把 `C:\`、`/home`、反斜杠或大小写假设写进 Renderer。当前 D1A 使用 `std::filesystem`、标准文件流、libuv 和 Mbed TLS 的统一接口，项目代码不直接调用 Win32、`openat` 或 Linux syscall。保存策略必须保持两平台的用户可观察保证，而不是要求底层系统调用名称相同。

如果后续安全保存、监听或回收站能力确实无法由标准库和现有跨平台依赖表达，应先用测试证明缺口并评审新的跨平台抽象。未经新的 ADR，不得在业务模块中新增 Windows/Linux 双分支、平台句柄类型或散落的条件编译。

## 1.7\_应用数据

应用数据使用 Electron 提供的用户数据目录，分为 `state/`、`backups/`、`history/`、`cache/` 和 `logs/`。绝对路径由 Main 获取，不由环境变量或工作区内容决定。

- 状态 Schema 版本化并支持失败回退。
- 备份与历史使用当前用户权限，不能放入临时公共目录。
- 清缓存只删除 `cache/`。
- 卸载和清应用数据永不删除用户打开的文件或文件夹。
- 工作区本身不自动写 `.loop/`、数据库或设置文件。

## 1.8\_打包与更新

打包方案必须通过最小 spike 验证：

- Main/Preload/Renderer/Worker 构建与 source map 边界。
- CodeMirror、Unified、Mermaid、KaTeX 的许可与包体积。
- Windows 安装/卸载、Ubuntu 包安装/卸载与桌面集成。
- 代码签名、更新签名、回滚和旧版本仍可读取恢复备份。
- 原生模块为零或被明确证明必要；第一阶段不因 SQLite 引入原生驱动。

更新器不能通过 Git 拉取源码或 npm 安装依赖。更新失败不修改用户工作区，数据库或状态迁移失败时旧版本仍可启动或进入明确恢复。

## 1.9\_迁移与删除边界

当前 `0.1.0` 仍是 Bash 启动的 Vite 浏览器训练工具。新桌面纵向闭环在独立入口和测试中完成后，删除：

- 浏览器正式入口和本地 HTTP 服务。
- Bash/MSYS2 最终用户安装、运行与补全链。
- IndexedDB 业务主存储。
- 训练 `banks`、专题电子书和对应 Schema/页面。
- 根仓库到工具的隐式启动与知识源配置依赖。

不维护双运行模式、双写存储、旧 URL 转发或旧内容包适配器。若交付前发现真实用户数据迁移需求，先列出数据类型、风险和一次性导出方案，由开发者确认后单独实施。

## 1.10\_平台验收矩阵

两个平台都必须在干净环境验证：

- 安装、启动、更新、回滚与卸载。
- 新建、打开单文件、打开文件夹、最近打开与 Hot Exit。
- UTF-8/BOM/换行、只读、磁盘满、占用、权限变化和外部修改。
- symlink/junction/reparse/mount 越界与文件身份变化。
- 回收站、资源协议、外部浏览器和网络默认阻止。
- 大文件、大目录、watcher 资源不足时的可见降级。
- 清缓存与卸载不删除用户文件、恢复备份或历史的错误范围。

只有安装后的生产包在 Windows 与 Ubuntu 都完成纵向闭环，才能宣布完成跨平台独立。

## 1.11\_相关设计

- [架构索引](architecture/README.md)
- [桌面运行时与文档服务设计](architecture/engineering/desktop_runtime_and_document_services.md)
- [桌面运行时安全与威胁模型](architecture/engineering/desktop_runtime_security_and_threat_model.md)
- [工作区文件操作与数据安全](architecture/engineering/workspace_file_operations_and_data_safety.md)
