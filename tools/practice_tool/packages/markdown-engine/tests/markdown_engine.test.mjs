import assert from "node:assert/strict";
import test from "node:test";
import {
  MAXIMUM_MERMAID_BLOCK_BYTES,
  MAXIMUM_PREVIEW_SOURCE_BYTES,
  PREVIEW_PROTOCOL_VERSION,
  is_preview_document,
  is_preview_source_request,
  parse_markdown_preview,
  preview_engine_error,
} from "../dist/index.js";

const document_id = "0123456789abcdef0123456789abcdef";

function request(source, revision = 0) {
  return {
    message_type: "parse_preview",
    protocol_version: PREVIEW_PROTOCOL_VERSION,
    document_id,
    revision,
    source,
  };
}

function collect_elements(document) {
  const elements = [];
  const stack = document.blocks.flatMap((block) => block.kind === "markdown" ? [...block.safe_hast.children] : []);
  while (stack.length > 0) {
    const node = stack.pop();
    if (node?.type === "element") {
      elements.push(node);
      stack.push(...node.children);
    }
  }
  return elements;
}

test("CommonMark 与 GFM 产生有序、可验证的 safe HAST 顶层块", () => {
  const source = [
    "# 标题",
    "",
    "- [x] 完成",
    "- [ ] 待办",
    "",
    "~~删除~~ 与 `code`",
    "",
    "| 左 | 右 |",
    "| :- | -: |",
    "| 一 | 二 |",
  ].join("\n");
  const result = parse_markdown_preview(request(source, 7));
  assert.equal(is_preview_document(result), true);
  assert.equal(result.protocol_version, 3);
  assert.equal(result.revision, 7);
  assert.equal(result.source_length, source.length);
  assert.ok(result.blocks.length >= 4);
  assert.equal(result.blocks.every((block, index) => index === 0
    || result.blocks[index - 1].source_span.end <= block.source_span.start), true);
  const elements = collect_elements(result);
  assert.ok(elements.some((node) => node.tagName === "h1"));
  assert.ok(elements.some((node) => node.tagName === "del"));
  assert.ok(elements.some((node) => node.tagName === "table"));
  assert.equal(elements.filter((node) => node.tagName === "input").length, 2);
});

test("Mermaid 围栏成为有界复杂块描述符且普通代码仍是 safe HAST", () => {
  const source = [
    "```mermaid",
    "flowchart TD",
    "  A[编辑] --> B[渲染]",
    "```",
    "",
    "```text",
    "flowchart TD",
    "```",
  ].join("\n");
  const result = parse_markdown_preview(request(source));
  assert.equal(result.blocks[0].kind, "mermaid");
  assert.match(result.blocks[0].mermaid_source, /A\[编辑\]/u);
  assert.equal(result.blocks[1].kind, "markdown");
  assert.equal(is_preview_document(result), true);
  const over_limit = "x".repeat(MAXIMUM_MERMAID_BLOCK_BYTES + 1);
  const degraded = parse_markdown_preview(request(`\`\`\`mermaid\n${over_limit}\n\`\`\``));
  assert.equal(degraded.blocks[0].kind, "markdown");
  assert.equal(degraded.diagnostics.some((item) => item.code === "MERMAID_BLOCK_TOO_LARGE"), true);
});

test("原始 HTML、远程图片和危险链接不会成为可执行节点", () => {
  const result = parse_markdown_preview(request([
    "<script>globalThis.compromised = true</script>",
    "",
    "![远程图](https://attacker.invalid/track.png)",
    "",
    "[危险](javascript:alert(1))",
    "",
    "[安全但保持不可导航](https://example.com/doc)",
  ].join("\n")));
  const serialized = JSON.stringify(result.blocks);
  assert.equal(serialized.includes("globalThis.compromised"), false);
  assert.equal(serialized.includes("attacker.invalid"), false);
  assert.equal(serialized.includes("javascript:"), false);
  const elements = collect_elements(result);
  assert.equal(elements.some((node) => node.tagName === "img"), false);
  assert.ok(elements.some((node) => node.tagName === "span" && node.properties.className?.includes("loop_resource_placeholder")));
  assert.ok(elements.some((node) => node.tagName === "a" && node.properties.href === "https://example.com/doc"));
  assert.deepEqual(new Set(result.diagnostics.map((item) => item.code)), new Set([
    "RAW_HTML_IGNORED",
    "REMOTE_RESOURCE_BLOCKED",
    "UNSAFE_LINK_REMOVED",
  ]));
});

test("预览请求拒绝未知字段、旧协议和无效修订", () => {
  assert.equal(is_preview_source_request({ ...request("x"), path: "C:\\secret.md" }), false);
  assert.equal(is_preview_source_request({ ...request("x"), revision: -1 }), false);
  assert.equal(is_preview_source_request({ ...request("x"), protocol_version: 1 }), false);
});

test("源码上限按 UTF-8 字节执行并允许精确 5 MiB", () => {
  const exact = "a".repeat(MAXIMUM_PREVIEW_SOURCE_BYTES);
  const result = parse_markdown_preview(request(exact));
  assert.equal(result.source_byte_length, MAXIMUM_PREVIEW_SOURCE_BYTES);
  assert.equal(is_preview_document(result), true);
  assert.throws(
    () => parse_markdown_preview(request(`${exact}a`)),
    (error) => error instanceof preview_engine_error && error.error_code === "PREVIEW_SOURCE_TOO_LARGE",
  );
});

test("深层 Markdown 在进入 HAST 转换前失败关闭", () => {
  const source = `${"> ".repeat(70)}内容`;
  assert.throws(
    () => parse_markdown_preview(request(source)),
    (error) => error instanceof preview_engine_error && error.error_code === "PREVIEW_TREE_TOO_DEEP",
  );
});

test("块协议校验拒绝跨度、标签、属性和计数篡改", () => {
  const result = parse_markdown_preview(request("# 安全"));
  const block = result.blocks[0];
  assert.equal(block.kind, "markdown");
  assert.equal(is_preview_document({ ...result, node_count: result.node_count + 1 }), false);
  assert.equal(is_preview_document({
    ...result,
    blocks: [{ ...block, source_span: { start: 0, end: result.source_length + 1 } }],
  }), false);
  assert.equal(is_preview_document({
    ...result,
    blocks: [{
      ...block,
      safe_hast: { type: "root", children: [{ type: "element", tagName: "script", properties: {}, children: [] }] },
      node_count: 2,
    }],
    node_count: 2,
  }), false);
  assert.equal(is_preview_document({
    ...result,
    blocks: [{
      ...block,
      safe_hast: { type: "root", children: [{ type: "element", tagName: "p", properties: { onclick: "run()" }, children: [] }] },
      node_count: 2,
    }],
    node_count: 2,
  }), false);
});
