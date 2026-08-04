import type { Element, Nodes, Root } from "hast";
import type { Nodes as mdast_node, Root as mdast_root, RootContent as mdast_root_content } from "mdast";
import remarkGfm from "remark-gfm";
import remarkParse from "remark-parse";
import remarkRehype from "remark-rehype";
import { unified } from "unified";
import {
  MAXIMUM_MERMAID_BLOCK_BYTES,
  MAXIMUM_MERMAID_BLOCKS,
  MAXIMUM_PREVIEW_BLOCKS,
  MAXIMUM_PREVIEW_DIAGNOSTICS,
  MAXIMUM_PREVIEW_SOURCE_BYTES,
  MAXIMUM_SAFE_HAST_BYTES,
  MAXIMUM_SAFE_HAST_DEPTH,
  MAXIMUM_SAFE_HAST_NODES,
  PREVIEW_PROTOCOL_VERSION,
  is_allowed_preview_link,
  is_preview_source_request,
  preview_engine_error,
  type preview_block,
  type preview_diagnostic,
  type preview_diagnostic_code,
  type preview_document,
  type preview_source_request,
  type safe_hast_node,
  type safe_hast_property,
  type safe_hast_root,
} from "./contracts.js";

const MAXIMUM_TEXT_NODE_BYTES = MAXIMUM_PREVIEW_SOURCE_BYTES;
const SAFE_TAG_NAMES = Object.freeze([
  "a", "blockquote", "br", "code", "del", "em", "h1", "h2", "h3", "h4", "h5", "h6",
  "hr", "input", "li", "ol", "p", "pre", "span", "strong", "table", "tbody", "td", "th",
  "thead", "tr", "ul",
] as const);
const SAFE_TAG_SET = new Set<string>(SAFE_TAG_NAMES);
const SAFE_ALIGNMENT_SET = new Set(["left", "center", "right"]);
const LANGUAGE_CLASS_PATTERN = /^language-[a-z0-9][a-z0-9_+-]{0,63}$/u;

const markdown_parser = unified().use(remarkParse).use(remarkGfm);
const hast_processor = unified()
  .use(remarkRehype)
  .use(() => replace_resource_elements);

function utf8_byte_length(value: string): number {
  return new TextEncoder().encode(value).byteLength;
}

function add_diagnostic(diagnostics: preview_diagnostic[], code: preview_diagnostic_code, message: string): void {
  if (diagnostics.length < MAXIMUM_PREVIEW_DIAGNOSTICS) diagnostics.push({ code, message });
}

function inspect_mdast(root: mdast_node, diagnostics: preview_diagnostic[]): void {
  const stack: Array<{ node: mdast_node; depth: number }> = [{ node: root, depth: 1 }];
  while (stack.length > 0) {
    const current = stack.pop();
    if (!current) break;
    if (current.depth > MAXIMUM_SAFE_HAST_DEPTH) {
      throw new preview_engine_error("PREVIEW_TREE_TOO_DEEP", "Markdown 嵌套超过安全预览上限");
    }
    if (current.node.type === "html") {
      add_diagnostic(diagnostics, "RAW_HTML_IGNORED", "原始 HTML 已按安全策略忽略");
    } else if (current.node.type === "image" || current.node.type === "imageReference") {
      add_diagnostic(diagnostics, "REMOTE_RESOURCE_BLOCKED", "图片资源保持阻止状态");
    } else if (current.node.type === "link" && !is_allowed_preview_link(current.node.url)) {
      add_diagnostic(diagnostics, "UNSAFE_LINK_REMOVED", "不安全或未知链接已从渲染块中移除");
    }
    if ("children" in current.node && Array.isArray(current.node.children)) {
      for (let index = current.node.children.length - 1; index >= 0; index -= 1) {
        const child = current.node.children[index];
        if (child) stack.push({ node: child, depth: current.depth + 1 });
      }
    }
  }
}

function replace_resource_elements(tree: Root): void {
  const parents: Array<Root | Element> = [tree];
  while (parents.length > 0) {
    const parent = parents.pop();
    if (!parent) break;
    for (let index = 0; index < parent.children.length; index += 1) {
      const child = parent.children[index];
      if (child?.type !== "element") continue;
      if (child.tagName === "img") {
        const alt = typeof child.properties.alt === "string" && child.properties.alt.length > 0
          ? child.properties.alt.slice(0, 512)
          : "图片";
        parent.children[index] = {
          type: "element",
          tagName: "span",
          properties: { className: ["loop_resource_placeholder"] },
          children: [{ type: "text", value: `[${alt}：资源加载已阻止]` }],
        };
      } else {
        parents.push(child);
      }
    }
  }
}

function canonical_properties(element: Element): Record<string, safe_hast_property> {
  const result: Record<string, safe_hast_property> = {};
  const properties = element.properties;
  if (element.tagName === "a" && typeof properties.href === "string" && is_allowed_preview_link(properties.href)) {
    result.href = properties.href;
  } else if (element.tagName === "ol" && typeof properties.start === "number" && Number.isSafeInteger(properties.start)) {
    result.start = properties.start;
  } else if (element.tagName === "input") {
    result.type = "checkbox";
    result.disabled = true;
    if (properties.checked === true) result.checked = true;
  } else if ((element.tagName === "td" || element.tagName === "th")
      && typeof properties.align === "string" && SAFE_ALIGNMENT_SET.has(properties.align)) {
    result.align = properties.align;
  }
  const class_name = properties.className;
  if (Array.isArray(class_name)) {
    const values = class_name.filter((value): value is string => typeof value === "string" && (
      (element.tagName === "code" && LANGUAGE_CLASS_PATTERN.test(value))
      || (element.tagName === "ul" && value === "contains-task-list")
      || (element.tagName === "li" && value === "task-list-item")
      || (element.tagName === "span" && value === "loop_resource_placeholder")
    ));
    if (values.length > 0) result.className = values;
  }
  return result;
}

function canonicalize_hast(root: Root): { root: safe_hast_root; node_count: number } {
  const safe_root: { type: "root"; children: safe_hast_node[] } = { type: "root", children: [] };
  const stack: Array<{ source_children: readonly Nodes[]; target_children: safe_hast_node[]; depth: number }> = [
    { source_children: root.children, target_children: safe_root.children, depth: 1 },
  ];
  let node_count = 1;
  while (stack.length > 0) {
    const current = stack.pop();
    if (!current) break;
    if (current.depth > MAXIMUM_SAFE_HAST_DEPTH) {
      throw new preview_engine_error("PREVIEW_TREE_TOO_DEEP", "安全渲染树嵌套超过上限");
    }
    for (const source_node of current.source_children) {
      if (source_node.type === "text") {
        node_count += 1;
        if (node_count > MAXIMUM_SAFE_HAST_NODES || utf8_byte_length(source_node.value) > MAXIMUM_TEXT_NODE_BYTES) {
          throw new preview_engine_error("PREVIEW_TREE_TOO_LARGE", "安全渲染树超过上限");
        }
        current.target_children.push({ type: "text", value: source_node.value });
      } else if (source_node.type === "element" && SAFE_TAG_SET.has(source_node.tagName)) {
        node_count += 1;
        if (node_count > MAXIMUM_SAFE_HAST_NODES) {
          throw new preview_engine_error("PREVIEW_TREE_TOO_LARGE", "安全渲染树节点数超过上限");
        }
        const target: {
          type: "element";
          tagName: string;
          properties: Record<string, safe_hast_property>;
          children: safe_hast_node[];
        } = {
          type: "element",
          tagName: source_node.tagName,
          properties: canonical_properties(source_node),
          children: [],
        };
        current.target_children.push(target);
        stack.push({ source_children: source_node.children, target_children: target.children, depth: current.depth + 1 });
      }
    }
  }
  return { root: safe_root, node_count };
}

function html_placeholder(): { root: safe_hast_root; node_count: number } {
  return {
    root: {
      type: "root",
      children: [{
        type: "element",
        tagName: "span",
        properties: { className: ["loop_resource_placeholder"] },
        children: [{ type: "text", value: "原始 HTML 已忽略；点击查看源码" }],
      }],
    },
    node_count: 3,
  };
}

function content_hash(source: string): string {
  let first = 0x811c9dc5;
  let second = 0x9e3779b9;
  for (let index = 0; index < source.length; index += 1) {
    const code = source.charCodeAt(index);
    first = Math.imul(first ^ code, 0x01000193) >>> 0;
    second = Math.imul(second ^ code, 0x85ebca6b) >>> 0;
  }
  return `${first.toString(16).padStart(8, "0")}${second.toString(16).padStart(8, "0")}`;
}

function source_span(node: mdast_root_content): { start: number; end: number } {
  const start = node.position?.start.offset;
  const end = node.position?.end.offset;
  if (typeof start !== "number" || typeof end !== "number" || end <= start) {
    throw new preview_engine_error("PREVIEW_PARSE_FAILED", "Markdown 块缺少有效源码位置");
  }
  return { start, end };
}

function markdown_block(node: mdast_root_content, block_index: number, source: string): preview_block {
  const span = source_span(node);
  const block_source = source.slice(span.start, span.end);
  let canonical: { root: safe_hast_root; node_count: number };
  if (node.type === "html") {
    canonical = html_placeholder();
  } else {
    const block_root: mdast_root = { type: "root", children: [node] };
    const unsafe_hast = hast_processor.runSync(block_root) as Root;
    canonical = canonicalize_hast(unsafe_hast);
  }
  const hash = content_hash(block_source);
  return {
    block_id: `block_${block_index}_${hash}`,
    source_span: span,
    content_hash: hash,
    kind: "markdown",
    safe_hast: canonical.root,
    node_count: canonical.node_count,
  };
}

export function parse_markdown_preview(request: preview_source_request): preview_document {
  if (!is_preview_source_request(request)) throw new preview_engine_error("INVALID_PREVIEW_REQUEST", "预览请求无效");
  const source_byte_length = utf8_byte_length(request.source);
  if (source_byte_length > MAXIMUM_PREVIEW_SOURCE_BYTES) {
    throw new preview_engine_error("PREVIEW_SOURCE_TOO_LARGE", "文档超过 5 MiB 安全渲染上限");
  }
  try {
    const diagnostics: preview_diagnostic[] = [];
    const mdast = markdown_parser.parse(request.source) as mdast_root;
    inspect_mdast(mdast, diagnostics);
    if (mdast.children.length > MAXIMUM_PREVIEW_BLOCKS) {
      throw new preview_engine_error("PREVIEW_TREE_TOO_LARGE", "Markdown 顶层块数量超过安全上限");
    }

    const blocks: preview_block[] = [];
    let mermaid_count = 0;
    let node_count = 0;
    for (let index = 0; index < mdast.children.length; index += 1) {
      const node = mdast.children[index];
      if (!node) continue;
      const span = source_span(node);
      const block_source = request.source.slice(span.start, span.end);
      const is_mermaid = node.type === "code" && node.lang?.trim().toLowerCase() === "mermaid";
      if (is_mermaid && utf8_byte_length(node.value) <= MAXIMUM_MERMAID_BLOCK_BYTES
          && mermaid_count < MAXIMUM_MERMAID_BLOCKS) {
        const hash = content_hash(block_source);
        blocks.push({
          block_id: `block_${index}_${hash}`,
          source_span: span,
          content_hash: hash,
          kind: "mermaid",
          mermaid_source: node.value,
        });
        mermaid_count += 1;
        continue;
      }
      if (is_mermaid && utf8_byte_length(node.value) > MAXIMUM_MERMAID_BLOCK_BYTES) {
        add_diagnostic(diagnostics, "MERMAID_BLOCK_TOO_LARGE", "Mermaid 块超过 256 KiB，已安全降级为源码代码块");
      } else if (is_mermaid) {
        add_diagnostic(diagnostics, "MERMAID_BLOCK_LIMIT_EXCEEDED", "Mermaid 块超过每文档 128 个上限，已安全降级为源码代码块");
      }
      const block = markdown_block(node, index, request.source);
      blocks.push(block);
      node_count += block.kind === "markdown" ? block.node_count : 0;
      if (node_count > MAXIMUM_SAFE_HAST_NODES) {
        throw new preview_engine_error("PREVIEW_TREE_TOO_LARGE", "安全渲染树节点数超过上限");
      }
    }
    if (utf8_byte_length(JSON.stringify(blocks)) > MAXIMUM_SAFE_HAST_BYTES) {
      throw new preview_engine_error("PREVIEW_TREE_TOO_LARGE", "安全渲染块序列化大小超过上限");
    }
    return {
      protocol_version: PREVIEW_PROTOCOL_VERSION,
      document_id: request.document_id,
      revision: request.revision,
      source_length: request.source.length,
      source_byte_length,
      blocks,
      diagnostics,
      node_count,
    };
  } catch (error) {
    if (error instanceof preview_engine_error) throw error;
    throw new preview_engine_error("PREVIEW_PARSE_FAILED", "Markdown 渲染解析失败");
  }
}
