# 显式行号

1. Typora 本身不自带“显式显示代码块行号”的功能，但可以通过 **自定义 CSS 主题** 来实现。下面给你一个完整的方案：

   ------


## 1. 找到 Typora 的主题目录

在 Typora 中：

- 打开菜单 **文件**→**偏好设置 → Markdown→ 代码块** →**勾选：显式行号** → **重启软件**
   里面有 `*.css` 文件（如 `github.css`、`night.css` 等）。

------

## 2. 修改主题 CSS

文件位置：`C:\Users\$(user)\AppData\Roaming\Typora\themes`。

由于每次typora更新会把自身携带的 主题（如 `github.css` ）给还原，因此需要在对应主题文件(如 `github.css` )复制出文件副本，重命名 `cpp_github.css` ，在副本文件的末尾添加以下样式：

```css
/* 给代码块加上行号 */
pre.md-fences {
  position: relative;
  counter-reset: line; /* 初始化行号 */
  padding-left: 3em;   /* 给行号留空间 */
}

pre.md-fences code {
  counter-reset: line;
}

pre.md-fences code span {
  display: block;
  counter-increment: line;
}

pre.md-fences code span::before {
  content: counter(line);
  display: inline-block;
  width: 2em;             /* 行号宽度 */
  margin-left: -3em;      /* 向左顶格 */
  text-align: right;
  color: #888;            /* 行号颜色 */
  user-select: none;      /* 不可复制 */
}
```

------

## 3. 启用行号效果

- 打开 Typora，切换到对应的主题（如 `Github`）。
- 在代码块内每一行都会自动显示行号。

------

## 4. 注意事项

1. Typora 渲染代码块时，每行内容会包在 `<span>` 里，所以样式选择器必须用 `code span`。
2. 如果某些主题不生效，说明该主题代码块结构不同，可以按 F12（开发者工具）检查 DOM。
3. 你可以额外美化，比如背景框、行号高亮当前行等。

------

要不要我帮你把 **完整的“代码高亮 + 行号主题（比如 GitHub 风格）”** 直接给你一份 CSS，你可以直接放到 Typora 里用？

# 标题颜色修改

我懂了 👍。你是希望在 **Typora** 里写 Markdown 时，标题（`# 一级标题`, `## 二级标题` …）显示成蓝色，而不是默认和正文一样的黑色。

Typora 本身就是用 CSS 渲染 Markdown 的，所以改 **主题 CSS** 就能改变标题颜色。

------

## 方法步骤

1. **找到主题目录**

   - Windows:

     ```
     C:\Users\<你的用户名>\AppData\Roaming\Typora\themes
     ```

   - Linux / macOS:

     ```
     ~/.config/Typora/themes/
     ```

   - 这里每个 `.css` 文件就是一个主题（比如 `night.css`, `github.css`）。

2. **复制一个主题**
    比如你平时用的是 `github.css`，可以复制一份改名成 `github-blue.css`，这样不会破坏原主题。

3. **修改标题颜色**
    打开你复制的 `.css` 文件，加入或修改以下样式（放到文件最后也行）：

   ```css
   h1, h2, h3, h4, h5, h6 {
       color: #0066cc; /* 标题颜色改为蓝色 */
   }
   ```

   - `#0066cc` 是深蓝色，你可以换成你喜欢的颜色，比如亮蓝 `#3399ff`。

4. **在 Typora 中选择新主题**
    打开 Typora → 菜单 **主题** → 选择 `github-blue`（就是你新建的主题）。

------

## 效果

- 原来标题是黑色 → 改为蓝色。
- 正文依然是黑色，不会混淆。

------

# 添加公式渲染选项

文件->偏好设置->Markdown->公式->开启内联公式和代码块公式



