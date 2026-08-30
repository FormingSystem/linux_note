# 第1章\_Typora\_语法与图表增强

本扩展补足主题 CSS 无法承担的两项能力：

- 使用 VS Code 内置 C/C++ TextMate 语法和 Oniguruma 解析 Typora 的 `c`、`cpp` 等代码围栏，并按 VS Code Light+ 的语法角色配色；
- 对超过响应式阅读高度的普通代码块默认限高，提供 `展开全部代码` / `收起代码` 切换，打印时自动完整展开且不修改 Markdown；
- 在 Mermaid 图正上方放置随正文自然滚动的静态工具行，不悬浮、不跟随视口，也不覆盖图；工具行挂在预览容器内部，并按 Mermaid 代码块去重，避免 Typora 的隐藏或重建预览产生重复按钮；
- 为 Typora 已渲染的 Mermaid SVG 增加独立全屏查看器，默认保持 `100%`，支持按钮缩放、`Ctrl + 滚轮` 指针中心缩放、左键拖动、适应宽度、适应屏幕、恢复 `100%` 和 `Esc` 退出。

扩展不按 `int`、`struct` 或 Linux API 名称写私有规则。VS Code grammar 负责产生 `entity.name.function.c`、`entity.name.type.c`、`variable.*` 等语法作用域，样式只映射作用域颜色。TextMate 是语法级解析，不是编译器或语言服务器；它不会读取项目头文件、宏展开结果和编译数据库，因此不宣称提供 VS Code C/C++ 扩展的完整语义分析。

## 1.1\_普通用户一键配置

仓库已经提交预构建 bundle，普通用户无需安装 Node.js。部署脚本不写死 Typora 安装位置；它先检查显式参数、`TYPORA_ROOT`、运行进程和系统发现信息，仍找不到时才询问用户。

Windows PowerShell 或资源管理器入口：

```text
tools\typora\configure_windows.cmd
```

MSYS2 UCRT64 或 Linux Bash 入口：

```bash
cd tools/typora
bash ./configure.sh
```

PowerShell 能识别 Windows、UCRT64 和 WSL 风格路径；UCRT64 Bash 能识别 Windows 与 POSIX 路径；Linux Bash 只接受 Linux 路径。脚本会先统一备份现有主题、Typora `resources/window.html` 和旧扩展 bundle，再安装仓库版本。完整安装、检查和回退说明见 [`../typora配置修改.md`](../typora配置修改.md#第6章_PowerShell、UCRT64与Linux一键配置)。

## 1.2\_开发者构建

```powershell
cd tools/typora/enhancements
npm install
npm run build
npm run check
```

`vendor/vscode_cpp/` 保存当前适配的 VS Code 内置 C/C++ grammar 和许可证说明。升级 grammar 后必须重新运行测试，至少确认 `rcu_dereference(table[id])` 中的函数调用仍产生 `entity.name.function.c`。

## 1.3\_PowerShell单独安装扩展与备份

安装脚本先把 Typora 的 `resources/window.html` 和旧扩展 bundle 备份到：

```text
%APPDATA%\Typora\backups\linux_note_typora_enhancements\<时间戳>\
```

随后只在 `window.html` 的 `</body>` 前加入一个用户数据脚本入口，主体 bundle 保存在 `%APPDATA%\Typora\linux_note_enhancements\`。执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install_windows.ps1
```

该脚本复用 `tools/typora/scripts/lib/typora_environment.ps1`，不维护自己的固定安装目录候选。Typora 更新会替换安装目录，更新后若入口消失，应重新运行安装脚本。不要在文档有未保存修改时强制退出 Typora；安装完成后保存文档并正常重启，扩展才会进入新窗口。

回退时传入安装输出的备份目录：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\restore_windows.ps1 `
  -backup_root "$env:APPDATA\Typora\backups\linux_note_typora_enhancements\<时间戳>"
```

恢复脚本在覆盖当前入口前还会再生成一份 `before_restore.bak` 安全副本。
