import fs from "node:fs";
import path from "node:path";
import vm from "node:vm";

const output = path.resolve("dist/typora_enhancements.js");
if (!fs.existsSync(output)) throw new Error("dist/typora_enhancements.js is missing; run npm run build");
const source = fs.readFileSync(output, "utf8");
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
console.log(`validated ${path.relative(process.cwd(), output)} (${source.length} bytes)`);
