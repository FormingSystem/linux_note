import { readdir, readFile } from "node:fs/promises";
import { join, relative, resolve } from "node:path";
import DOMPurify from "dompurify";
import mermaid from "mermaid";

// Mermaid 的语法解析器在 Node 中也会调用浏览器版 DOMPurify 钩子。
// 此脚本只解析本地源码、不生成或注入 HTML，因此用无副作用接口补齐 Node 环境。
if (typeof DOMPurify.addHook !== "function") DOMPurify.addHook = () => {};
if (typeof DOMPurify.removeHook !== "function") DOMPurify.removeHook = () => {};
if (typeof DOMPurify.sanitize !== "function") DOMPurify.sanitize = (value) => value;

const repositoryRoot = resolve(process.cwd(), "../..");
const ignoredDirectories = new Set([".git", ".local", "dist", "node_modules"]);
const failures = [];
let diagramCount = 0;
let fileCount = 0;

for await (const file of markdownFiles(repositoryRoot)) {
  fileCount += 1;
  const markdown = await readFile(file, "utf8");
  for (const diagram of mermaidBlocks(markdown)) {
    diagramCount += 1;
    try {
      await mermaid.parse(diagram.source);
    } catch (error) {
      failures.push({
        file: relative(repositoryRoot, file).replaceAll("\\", "/"),
        line: diagram.line,
        message: error instanceof Error ? error.message.split("\n")[0] : String(error),
      });
    }
  }
}

if (failures.length) {
  console.error(`Mermaid 检查失败：${failures.length} 个图表无法解析。`);
  failures.forEach((failure) => console.error(`- ${failure.file}:${failure.line} ${failure.message}`));
  process.exitCode = 1;
} else {
  console.log(`Mermaid 检查通过：扫描 ${fileCount} 个 Markdown 文件，验证 ${diagramCount} 个图表。`);
}

async function* markdownFiles(directory) {
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    if (entry.isDirectory() && ignoredDirectories.has(entry.name)) continue;
    const path = join(directory, entry.name);
    if (entry.isDirectory()) yield* markdownFiles(path);
    else if (entry.isFile() && entry.name.toLowerCase().endsWith(".md")) yield path;
  }
}

function mermaidBlocks(markdown) {
  const lines = markdown.replaceAll("\r\n", "\n").split("\n");
  const blocks = [];
  for (let index = 0; index < lines.length; index += 1) {
    const opening = lines[index].match(/^\s*(`{3,}|~{3,})\s*mermaid\s*$/i);
    if (!opening) continue;
    const marker = opening[1][0];
    const minimumLength = opening[1].length;
    const source = [];
    const line = index + 2;
    index += 1;
    const closing = new RegExp(`^\\s*${escapeRegExp(marker)}{${minimumLength},}\\s*$`);
    while (index < lines.length && !closing.test(lines[index])) {
      source.push(lines[index]);
      index += 1;
    }
    blocks.push({ line, source: source.join("\n") });
  }
  return blocks;
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
