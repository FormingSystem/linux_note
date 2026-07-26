---
id: tools.practice_tool.environment_and_troubleshooting
title: "知识训练工具环境与故障排查"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_知识训练工具环境与故障排查

本文统一说明“回路”知识训练工具在 Windows CMD、MSYS2/UCRT64 和 Linux 中的独立启动方式、首次环境准备、当前 `linux-note` 集成方式、虚拟机验证以及常见故障。

跨平台职责、知识源协议、仓库所有权和独立拆仓验收标准见：[跨平台与仓库独立性设计](cross_platform_and_repository_independence.md)。

## 1.1\_先识别当前终端

训练工具自身的正式入口是工具目录中的 `start.cmd` 和 `start.sh`。当前知识库根目录的 `practice.cmd`、`practice.sh` 只是快捷转发。不同终端不能混用命令语法。

查看工具介绍和全部启动选项时，可在 Linux 使用 `./start.sh --help`，在 Windows 使用 `start.cmd --help`。帮助参数在环境检查之前返回，不触发任何下载、安装或服务启动。

Linux Bash 补全使用工具自己的正式入口：

```bash
./start.sh --install-completion
```

安装结果保存在当前用户的 Bash completion 目录，不修改系统目录。默认目标是指向当前仓库 `scripts/completions/start.bash` 的符号链接；不支持符号链接时使用动态加载器。两种方式都不复制静态补全快照，因此当前目录完成 Git 更新后会自动使用新版规则。重新打开 Bash 后，输入 `./start.sh --` 并按 Tab 即可补全；当前终端可执行 `source <(./start.sh --completion bash)` 立即加载。

| 终端提示符示例 | 环境 | 启动命令 |
| --- | --- | --- |
| `F:\...\practice_tool>` | Windows CMD | `start` |
| `PS F:\...\practice_tool>` | PowerShell | `.\start.cmd` |
| `Lizha@host UCRT64 /f/.../practice_tool $` | MSYS2/UCRT64 | `./start.sh` |
| `user@host:~/practice_tool$` | Linux Bash | `./start.sh` |

终端提示符、命令输出和错误信息不能作为命令粘贴。例如下面这些内容不应输入：

```text
Lizha@host UCRT64 ...
$
-bash: ...
```

## 1.2\_最简启动方式

### 1.2.1\_Windows

在资源管理器中双击训练工具目录中的：

```text
start.cmd
```

也可以在 CMD 中执行：

```cmd
cd path\to\practice_tool
start
```

### 1.2.2\_MSYS2与Linux

在训练工具目录执行：

```bash
cd path/to/practice_tool
./start.sh
```

如果训练工具仍位于当前知识库中，也可以从仓库根目录使用 `practice.cmd` 或 `practice.sh` 快捷启动。两个根目录脚本只转发参数和退出状态，不包含工具启动逻辑。

启动器会显示本地地址，并在存在桌面环境时尝试打开默认浏览器：

```text
http://127.0.0.1:5173/
```

结束使用时，在运行服务的终端按 `Ctrl+C`。

### 1.2.3\_主动升级

普通启动只在 Node.js 缺失或低于最低兼容线时下载运行时。需要主动检查配置源中的更新并刷新依赖时：

```cmd
start.cmd --upgrade
```

```bash
./start.sh --upgrade
```

若工具仍位于 `linux-note` 中，也应优先进入 `tools/practice_tool` 后使用正式入口；外层快捷脚本不属于工具命令接口。

升级模式会重新选择官方最高可用兼容 Node.js、删除本机环境就绪标记、重新执行 `npm install`，然后启动 Vite。它不执行 `git pull`，也不更新题库、知识源配置、知识正文或用户作答。

联网下载采用“本国或就近镜像优先、境外官方源兜底”的顺序。默认先尝试 `https://npmmirror.com/mirrors/node` 和 `https://registry.npmmirror.com`，不可用时再尝试 `https://nodejs.org/dist` 和 `https://registry.npmjs.org`。镜像提供的 Node.js 包仍必须通过随发行版提供的 `SHASUMS256.txt` 摘要校验；npm 仓库仅通过当前安装命令的 `--registry` 指定，不写入全局配置。

跨国家使用、自建镜像或组织内网环境可以临时覆盖源列表，地址之间使用空格分隔，排列顺序就是尝试顺序：

```bash
PRACTICE_NODE_DIST_SOURCES="https://内网-node-镜像 https://nodejs.org/dist" \
PRACTICE_NPM_REGISTRIES="https://内网-npm-镜像 https://registry.npmjs.org" \
./start.sh --upgrade
```

Windows CMD 使用 `set PRACTICE_NODE_DIST_SOURCES=...` 和 `set PRACTICE_NPM_REGISTRIES=...` 后再执行 `start.cmd --upgrade`。

### 1.2.4\_离线准备Node.js

有网机器从 Node.js 官方 `https://nodejs.org/dist/` 下载目标平台归档和相同版本目录中的 `SHASUMS256.txt`。两者放入离线设备的：

```text
tools/practice_tool/.local/downloads/node/v<完整版本>/
```

Ubuntu x64 示例：

```text
tools/practice_tool/.local/downloads/node/v24.18.0/
├── SHASUMS256.txt
└── node-v24.18.0-linux-x64.tar.gz
```

Windows x64 示例：

```text
tools/practice_tool/.local/downloads/node/v24.18.0/
├── SHASUMS256.txt
└── node-v24.18.0-win-x64.zip
```

然后在工具目录执行 `./start.sh --upgrade` 或 `start.cmd --upgrade`。安装器按主版本优先级搜索缓存并强制校验，不需要访问网络。`.local/` 已被工具 `.gitignore` 排除，可通过 U 盘或共享目录复制这些文件，不应提交到仓库。

安装开始时有 5 秒选择窗口：

```text
A / 超时：自动选择官方在线版本，并优先复用缓存
M：手动输入一个归档和 SHASUMS256.txt 路径
T：读取离线包表
```

离线包表模板是 `config/offline_node_packages.example.tsv`。首次选择表格但本地表不存在时，程序会生成 `config/offline_node_packages.local.tsv` 并提示修改。将目标行的 `enabled` 改为 `1`；`archive`、`checksums` 均支持相对于 `practice_tool` 根目录的路径和绝对路径。

安装器根据文件后缀选择解压方式：

| 平台 | 后缀 | 要求 |
| --- | --- | --- |
| Linux | `.tar.gz`、`.tgz` | `tar` 与 `gzip` |
| Linux | `.tar.xz` | `tar` 与 `xz` 支持 |
| Windows | `.zip` | PowerShell `Expand-Archive` |
| Windows | `.7z` | 系统可找到 `7z.exe` 或 `7za.exe` |

后缀可变不代表可以任意改名。文件仍须遵循 Node.js 官方 `node-v<版本>-<平台>-<架构>.<后缀>` 命名，校验文件必须包含该文件的官方摘要，解压结果必须具有官方便携包的目录结构。

## 1.3\_第一次启动发生什么

第一次启动执行以下流程：

```text
寻找可用的 Node.js 和 npm，并检查主版本
    ↓
缺失或低于 v18 时按优先级准备官方兼容版本
    ↓
安装 practice_tool 项目依赖
    ↓
写入 .local/environment-ready-v3-node-compatible
    ↓
启动 Vite 本地服务
    ↓
打开默认浏览器
```

环境就绪标记和 `node_modules` 同时存在时，后续启动跳过依赖安装。如果 `node_modules` 被删除，启动器会自动重新准备依赖。Node.js 版本在每次启动时都会检查，旧版本不能绕过门禁。

不同环境的安装策略为：

| 环境 | 安装方式 | 是否使用 `sudo` |
| --- | --- | --- |
| Windows | 复用 Node.js 18 以上版本；优先 `winget`，官方 ZIP 兜底 | 否 |
| MSYS2/UCRT64 | 复用兼容版本，或用 MSYS2 `pacman` 安装对应 Node.js 包 | 否 |
| 普通 Linux | 从官方地址按 `24 → 22 → 20 → 18` 选择最高可用版本并校验，安装到 `.local/runtime` | 否 |

普通 Linux 不再直接采用发行版仓库中的 Node.js，因为 Ubuntu 等系统可能提供已经不能运行当前 Vite 的旧版本。隔离运行时不改变 `/usr/bin/node`，删除工具 `.local/runtime` 即可移除。

## 1.4\_平台内的使用顺序

打开平台后：

1. 在单元选择页按领域筛选，或搜索模块名、知识点和标签。
2. 选择训练单元，例如“RCU：从读侧扩展性到宽限期证明”。
3. 阅读单元概览。
4. 完成轻量场景提示提问。
5. 在无知识提示条件下完成小模块脱稿输出。
6. 分析专业工程案例，回答诊断、方案、不可规避成本和选择边界。
7. 根据首次独立输出选择“需要重建”“部分输出”或“完整输出”。

看到提示后能够理解，不等于脱稿掌握。评分应以打开提示之前的输出为依据。

## 1.5\_独立仓库边界

工具运行只要求工具目录自身包含 `package.json`、`src`、`banks`、`schemas`、`scripts` 和 `start.*`。根目录快捷入口不是依赖，复制或迁移工具时也不应把外层 `practice.*` 当成工具文件。

当前 RCU 内容包中的 `knowledge_refs.path` 指向 `linux-note` 的知识正文，用于记录来源和在集成环境中定位文档。程序加载题库依赖稳定 ID 和 `banks` 内文件，不应根据外层仓库目录推导训练单元。未来独立拆仓时需要为外部正文位置提供内容包配置或解析器，但不需要重写训练项目 ID。

知识源通过 `PRACTICE_SOURCE_CONFIG` 指定。Windows 和 Linux 使用同一个变量名，配置文件使用同一个 JSON Schema：

```powershell
$env:PRACTICE_SOURCE_CONFIG = "D:\knowledge\practice.sources.json"
.\start.cmd
```

```bash
PRACTICE_SOURCE_CONFIG=/opt/knowledge/practice.sources.json ./start.sh
```

配置中的 `filesystem` 相对地址以配置文件所在目录为基准，不以当前终端目录为基准。也可以把配置地址和 `http` 知识源地址写成 HTTP/HTTPS URL；远程配置不能声明启动机器上的 `filesystem` 地址。工具没有配置知识源时仍能运行题库，但会把相关来源标记为未配置。

当前 `linux-note` 根目录的 `practice.sources.json` 只声明本仓库是一个知识源。根快捷脚本仅在 `PRACTICE_SOURCE_CONFIG` 尚未设置时选择它，因此不会覆盖用户传入的其他知识库配置。

独立化检查至少包括：

```text
从工具目录直接启动
    ↓
不经过外层快捷脚本完成环境准备
    ↓
执行题库校验与前端构建
    ↓
确认程序代码没有访问外层相对路径
    ↓
单独处理内容包中的外部知识来源
```

## 1.6\_Linux虚拟机验证流程

### 1.6.1\_先确认改动已经推送

虚拟机只能克隆远端已经提交并推送的内容。本机未提交文件不会通过 `git clone` 出现在虚拟机中。

在本机检查：

```bash
git status
git status -sb
```

确认训练工具自身的 `start.sh`、环境脚本和文档已经进入目标提交并推送到远端后，再在虚拟机测试。根目录快捷入口不属于训练工具独立运行的必要文件。

### 1.6.2\_优先使用SSH克隆

已经配置 GitHub SSH 密钥时：

```bash
cd ~/linux
git clone --depth 1 --filter=blob:none \
  git@github.com:FormingSystem/linux_note.git
```

出现 `remote: Enumerating objects`、`Counting objects` 和 `Compressing objects` 表示远端已经开始传输。`^C` 表示用户按下 `Ctrl+C` 主动中断，不是 GitHub 拒绝连接。

### 1.6.3\_克隆后启动

```bash
cd ~/linux/linux_note
cd tools/practice_tool
./start.sh
```

有桌面环境时会尝试打开默认浏览器。无桌面的服务器环境只启动本地服务，需要根据虚拟机网络方式决定是否开放监听地址和端口；默认 `127.0.0.1` 只允许虚拟机内部访问。

## 1.7\_HTTPS代理故障

出现下面的错误：

```text
Failed to connect to 192.168.31.196 port 10808
No route to host
```

表示 Git 正在使用该代理地址，并不表示 GitHub 地址本身错误。

### 1.7.1\_查找代理来源

在虚拟机执行：

```bash
git config --show-origin --get-regexp 'http\..*proxy|https\..*proxy'
env | grep -iE '^(http|https|all)_proxy='
```

代理可能来自 Git 系统配置、用户配置、仓库配置或环境变量。

### 1.7.2\_确认Windows当前地址

在 Windows 执行：

```cmd
ipconfig
```

例如当前主机地址可能是：

```text
192.168.31.197
```

该地址可能因 DHCP、网卡或网络切换发生变化，不应写死到仓库脚本。

### 1.7.3\_从虚拟机测试端口

```bash
ip route get 192.168.31.197
nc -vz 192.168.31.197 10808
```

如果端口不可达，需要检查：

- 虚拟机是 NAT、桥接还是仅主机网络。
- Windows 代理软件是否启用“允许局域网连接”或 `Allow LAN`。
- 代理是否只监听 `127.0.0.1`，而没有监听局域网地址。
- Windows 防火墙是否允许对应端口入站。

### 1.7.4\_识别代理协议

测试 HTTP 代理：

```bash
curl -I -x http://192.168.31.197:10808 https://github.com
```

测试 SOCKS5 代理：

```bash
curl -I --proxy socks5h://192.168.31.197:10808 https://github.com
```

只有测试成功后，才应把相同协议用于 Git。排障阶段优先使用单次命令参数，不要立即覆盖全局 Git 配置。

HTTP 代理示例：

```bash
git -c http.proxy=http://192.168.31.197:10808 \
    -c https.proxy=http://192.168.31.197:10808 \
    clone --depth 1 --filter=blob:none \
    https://github.com/FormingSystem/linux_note.git
```

SOCKS5 代理示例：

```bash
git -c http.proxy=socks5h://192.168.31.197:10808 \
    -c https.proxy=socks5h://192.168.31.197:10808 \
    clone --depth 1 --filter=blob:none \
    https://github.com/FormingSystem/linux_note.git
```

## 1.8\_常见错误速查

| 现象 | 原因 | 处理方式 |
| --- | --- | --- |
| 大量 `npm WARN EBADENGINE`，显示 Node.js `v12` | 启动器复用了不兼容的系统 Node.js | 更新启动器；它会从官方地址选择最高可用兼容版本 |
| npm 长时间停在 `reify` 或 `http fetch` | 首次下载较慢，终端仍在安装依赖 | 观察下载耗时；单次请求超过 120 秒会失败并重试 |
| Firefox 提示无法连接 `127.0.0.1:5173` | 旧启动器在 Vite 尚未监听时提前打开浏览器 | 更新启动器；现在由 Vite 监听成功后执行 `--open` |
| 官方下载全部失败 | 网络、代理或目标架构归档不可用 | 按终端提示把官方归档与 `SHASUMS256.txt` 放入 `.local/downloads/node/v<版本>/` |
| CMD 提示 `'.' 不是内部或外部命令` | 在 CMD 中使用了 Bash 的 `./` | 在工具目录执行 `start` |
| Bash 报 `toolspractice_tool` 不存在 | 使用反斜杠，反斜杠被解释为转义 | 使用 `/` |
| Bash 报 `npm: command not found` | Node.js 目录不在当前 PATH | 使用工具目录的 `start.sh` 自动处理 |
| MSYS2 要求启用 Windows Sudo | 脚本错误进入 Linux 权限分支 | 使用已修正脚本；MSYS2 直接调用 `pacman` |
| 进入 `F:\...\practice_tool>` CMD 提示符 | `cmd.exe /c` 被 MSYS2转换为路径参数 | 使用已修正的 `//d //c` 调用方式 |
| Vite 报 `"node" 不是内部或外部命令` | 只找到 `npm.cmd`，未把同目录 `node.exe` 加入 PATH | 使用已修正启动器统一加入 Node.js 目录 |
| npm 显示 `fund` 或 `allow-scripts` | npm 信息或警告 | 只要 Vite 显示 `ready`，不影响启动 |
| 浏览器没有自动打开 | 无桌面环境或系统缺少打开工具 | 手动访问终端显示的地址 |

## 1.9\_重新准备环境

仅在依赖损坏或环境要求升级时执行。删除：

```text
tools/practice_tool/.local/environment-ready-v3-node-compatible
tools/practice_tool/node_modules/
tools/practice_tool/.local/runtime/
```

然后重新运行工具目录的 `start.cmd` 或 `start.sh`。不要把 `.local`、`node_modules` 或 `dist` 提交到 Git。
