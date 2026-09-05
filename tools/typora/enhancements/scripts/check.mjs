import fs from "node:fs";
import path from "node:path";
import vm from "node:vm";

const output = path.resolve("dist/typora_enhancements.js");
if (!fs.existsSync(output)) throw new Error("dist/typora_enhancements.js is missing; run npm run build");
const source = fs.readFileSync(output, "utf8");
const enhancement_css = fs.readFileSync(path.resolve("src/typora_enhancements.css"), "utf8");
const typora_theme = fs.readFileSync(path.resolve("../cpp_github-consolas.css"), "utf8");
new vm.Script(source, { filename: output });
for (const marker of [
  "linux-note-vscode-textmate-c",
  "linux-note-vscode-textmate-cpp",
  "linux-note-mermaid-viewer",
  "linux-note-mermaid-inline-toolbar",
  "linux-note-code-collapsible",
  "linux-note-code-toggle",
  "is-code-collapsed",
  "preview.prepend(toolbar)",
  "mermaid_container_for_preview",
  "fit-width",
  "data-linux-note-typora-enhancements",
]) {
  if (!source.includes(marker)) throw new Error(`bundle marker is missing: ${marker}`);
}
if (source.includes("schedule_mermaid_positions")) {
  throw new Error("obsolete viewport-following Mermaid positioner is still bundled");
}
for (const marker of [
  "--linux-note-code-foreground: #24292e;",
  "--linux-note-code-comment: #6a737d;",
  "--linux-note-code-string: #032f62;",
  "--linux-note-code-keyword: #d73a49;",
  "--linux-note-code-function: #6f42c1;",
  "--linux-note-code-variable: #005cc5;",
]) {
  if (!enhancement_css.includes(marker)) throw new Error(`GitHub Light enhancement color is missing: ${marker}`);
  if (!source.includes(marker)) throw new Error(`bundled GitHub Light color is missing: ${marker}`);
}
for (const marker of [
  "background-color: #f6f8fa;",
  "--code-keyword-color: #d73a49;",
  "--code-comment-color: #6a737d;",
  "--code-string-color: #032f62;",
  "--code-definition-color: #6f42c1;",
  "--code-variable-color: #005cc5;",
]) {
  if (!typora_theme.includes(marker)) throw new Error(`GitHub Light Typora theme color is missing: ${marker}`);
}
console.log(`validated ${path.relative(process.cwd(), output)} (${source.length} bytes)`);
