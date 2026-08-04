export const PREVIEW_PROTOCOL_VERSION = 3 as const;
export const MAXIMUM_PREVIEW_SOURCE_BYTES = 5 * 1024 * 1024;
export const MAXIMUM_SAFE_HAST_BYTES = 12 * 1024 * 1024;
export const MAXIMUM_SAFE_HAST_NODES = 100_000;
export const MAXIMUM_SAFE_HAST_DEPTH = 64;
export const MAXIMUM_PREVIEW_BLOCKS = 50_000;
export const MAXIMUM_PREVIEW_DIAGNOSTICS = 64;
export const MAXIMUM_PREVIEW_URL_LENGTH = 4_096;
export const MAXIMUM_MERMAID_BLOCK_BYTES = 256 * 1024;
export const MAXIMUM_MERMAID_BLOCKS = 128;

const MAXIMUM_DOCUMENT_ID_LENGTH = 160;
const MAXIMUM_BLOCK_ID_LENGTH = 96;
const MAXIMUM_TEXT_NODE_BYTES = MAXIMUM_PREVIEW_SOURCE_BYTES;
const SAFE_TAG_SET = new Set([
  "a", "blockquote", "br", "code", "del", "em", "h1", "h2", "h3", "h4", "h5", "h6",
  "hr", "input", "li", "ol", "p", "pre", "span", "strong", "table", "tbody", "td", "th",
  "thead", "tr", "ul",
]);
const SAFE_ALIGNMENT_SET = new Set(["left", "center", "right"]);
const LANGUAGE_CLASS_PATTERN = /^language-[a-z0-9][a-z0-9_+-]{0,63}$/u;
const CONTENT_HASH_PATTERN = /^[a-f0-9]{16}$/u;

export type preview_diagnostic_code =
  | "RAW_HTML_IGNORED"
  | "REMOTE_RESOURCE_BLOCKED"
  | "UNSAFE_LINK_REMOVED"
  | "MERMAID_BLOCK_TOO_LARGE"
  | "MERMAID_BLOCK_LIMIT_EXCEEDED";

export interface preview_diagnostic {
  code: preview_diagnostic_code;
  message: string;
}

export interface safe_hast_text {
  type: "text";
  value: string;
}

export type safe_hast_property = string | number | boolean | readonly string[];

export interface safe_hast_element {
  type: "element";
  tagName: string;
  properties: Readonly<Record<string, safe_hast_property>>;
  children: readonly safe_hast_node[];
}

export type safe_hast_node = safe_hast_text | safe_hast_element;

export interface safe_hast_root {
  type: "root";
  children: readonly safe_hast_node[];
}

export interface preview_source_span {
  start: number;
  end: number;
}

interface preview_block_base {
  block_id: string;
  source_span: preview_source_span;
  content_hash: string;
}

export interface preview_markdown_block extends preview_block_base {
  kind: "markdown";
  safe_hast: safe_hast_root;
  node_count: number;
}

export interface preview_mermaid_block extends preview_block_base {
  kind: "mermaid";
  mermaid_source: string;
}

export type preview_block = preview_markdown_block | preview_mermaid_block;

export interface preview_source_request {
  message_type: "parse_preview";
  protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
  document_id: string;
  revision: number;
  source: string;
}

export interface preview_document {
  protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
  document_id: string;
  revision: number;
  source_length: number;
  source_byte_length: number;
  blocks: readonly preview_block[];
  diagnostics: readonly preview_diagnostic[];
  node_count: number;
}

export interface preview_worker_success {
  message_type: "preview_result";
  document: preview_document;
}

export type preview_failure_code =
  | "INVALID_PREVIEW_REQUEST"
  | "PREVIEW_SOURCE_TOO_LARGE"
  | "PREVIEW_TREE_TOO_DEEP"
  | "PREVIEW_TREE_TOO_LARGE"
  | "PREVIEW_PARSE_FAILED";

export interface preview_worker_failure {
  message_type: "preview_failure";
  protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
  document_id: string;
  revision: number;
  error_code: preview_failure_code;
  user_message: string;
}

export type preview_worker_response = preview_worker_success | preview_worker_failure;

export class preview_engine_error extends Error {
  readonly error_code: preview_failure_code;

  constructor(error_code: preview_failure_code, message: string) {
    super(message);
    this.name = "preview_engine_error";
    this.error_code = error_code;
  }
}

function utf8_byte_length(value: string): number {
  return new TextEncoder().encode(value).byteLength;
}

function is_record(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function has_exact_keys(value: Record<string, unknown>, keys: readonly string[]): boolean {
  const actual_keys = Object.keys(value);
  return actual_keys.length === keys.length && keys.every((key) => Object.hasOwn(value, key));
}

function is_document_id(value: unknown): value is string {
  return typeof value === "string" && value.length >= 16 && value.length <= MAXIMUM_DOCUMENT_ID_LENGTH
    && /^[a-zA-Z0-9_-]+$/u.test(value);
}

function is_block_id(value: unknown): value is string {
  return typeof value === "string" && value.length >= 8 && value.length <= MAXIMUM_BLOCK_ID_LENGTH
    && /^[a-zA-Z0-9_-]+$/u.test(value);
}

function is_revision(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value >= 0;
}

export function is_preview_source_request(value: unknown): value is preview_source_request {
  return is_record(value)
    && has_exact_keys(value, ["message_type", "protocol_version", "document_id", "revision", "source"])
    && value.message_type === "parse_preview"
    && value.protocol_version === PREVIEW_PROTOCOL_VERSION
    && is_document_id(value.document_id)
    && is_revision(value.revision)
    && typeof value.source === "string";
}

function is_preview_diagnostic(value: unknown): value is preview_diagnostic {
  return is_record(value)
    && has_exact_keys(value, ["code", "message"])
    && (value.code === "RAW_HTML_IGNORED" || value.code === "REMOTE_RESOURCE_BLOCKED"
      || value.code === "UNSAFE_LINK_REMOVED" || value.code === "MERMAID_BLOCK_TOO_LARGE"
      || value.code === "MERMAID_BLOCK_LIMIT_EXCEEDED")
    && typeof value.message === "string" && value.message.length > 0 && value.message.length <= 256;
}

function are_safe_properties(tag_name: string, properties: Record<string, unknown>): boolean {
  const keys = Object.keys(properties);
  if (keys.length > 3) return false;
  for (const [key, property] of Object.entries(properties)) {
    if (tag_name === "a" && key === "href") {
      if (typeof property !== "string" || !is_allowed_preview_link(property)) return false;
    } else if (tag_name === "ol" && key === "start") {
      if (typeof property !== "number" || !Number.isSafeInteger(property)) return false;
    } else if (tag_name === "input" && key === "type") {
      if (property !== "checkbox") return false;
    } else if (tag_name === "input" && (key === "checked" || key === "disabled")) {
      if (property !== true) return false;
    } else if ((tag_name === "td" || tag_name === "th") && key === "align") {
      if (typeof property !== "string" || !SAFE_ALIGNMENT_SET.has(property)) return false;
    } else if (key === "className" && Array.isArray(property) && property.length <= 2
        && property.every((item) => typeof item === "string" && (
          (tag_name === "code" && LANGUAGE_CLASS_PATTERN.test(item))
          || (tag_name === "ul" && item === "contains-task-list")
          || (tag_name === "li" && item === "task-list-item")
          || (tag_name === "span" && item === "loop_resource_placeholder")
        ))) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}

function validate_safe_hast(value: unknown): { valid: boolean; node_count: number } {
  if (!is_record(value) || !has_exact_keys(value, ["type", "children"])
      || value.type !== "root" || !Array.isArray(value.children)) return { valid: false, node_count: 0 };
  const stack: Array<{ node: unknown; depth: number }> = value.children.map((node) => ({ node, depth: 1 }));
  let node_count = 1;

  while (stack.length > 0) {
    const current = stack.pop();
    if (!current || current.depth > MAXIMUM_SAFE_HAST_DEPTH || !is_record(current.node)) {
      return { valid: false, node_count };
    }
    node_count += 1;
    if (node_count > MAXIMUM_SAFE_HAST_NODES) return { valid: false, node_count };
    if (current.node.type === "text") {
      if (!has_exact_keys(current.node, ["type", "value"])
          || typeof current.node.value !== "string"
          || utf8_byte_length(current.node.value) > MAXIMUM_TEXT_NODE_BYTES) return { valid: false, node_count };
      continue;
    }
    if (current.node.type !== "element"
        || !has_exact_keys(current.node, ["type", "tagName", "properties", "children"])
        || typeof current.node.tagName !== "string" || !SAFE_TAG_SET.has(current.node.tagName)
        || !is_record(current.node.properties) || !Array.isArray(current.node.children)
        || !are_safe_properties(current.node.tagName, current.node.properties)) return { valid: false, node_count };
    for (const child of current.node.children) stack.push({ node: child, depth: current.depth + 1 });
  }
  return { valid: true, node_count };
}

export function is_safe_hast_root(value: unknown): value is safe_hast_root {
  return validate_safe_hast(value).valid;
}

export function is_allowed_preview_link(value: string): boolean {
  if (value.length === 0 || value.length > MAXIMUM_PREVIEW_URL_LENGTH || /[\u0000-\u001f\u007f]/u.test(value)) {
    return false;
  }
  if (value.startsWith("#") || value.startsWith("./") || value.startsWith("../") || value.startsWith("/")) {
    return true;
  }
  try {
    const parsed = new URL(value);
    return (parsed.protocol === "https:" || parsed.protocol === "http:") && !parsed.username && !parsed.password;
  } catch {
    return !value.startsWith("//") && !/^[a-z][a-z0-9+.-]*:/iu.test(value);
  }
}

function validate_preview_block(
  value: unknown,
  source_length: number,
  previous_end: number,
): { valid: boolean; end: number; node_count: number; mermaid: boolean } {
  if (!is_record(value) || !is_block_id(value.block_id)
      || typeof value.content_hash !== "string" || !CONTENT_HASH_PATTERN.test(value.content_hash)
      || !is_record(value.source_span)
      || !has_exact_keys(value.source_span, ["start", "end"])) {
    return { valid: false, end: previous_end, node_count: 0, mermaid: false };
  }
  const start = value.source_span.start;
  const end = value.source_span.end;
  if (typeof start !== "number" || !Number.isSafeInteger(start) || start < previous_end
      || typeof end !== "number" || !Number.isSafeInteger(end) || end <= start || end > source_length) {
    return { valid: false, end: previous_end, node_count: 0, mermaid: false };
  }
  if (value.kind === "markdown") {
    if (!has_exact_keys(value, ["block_id", "source_span", "content_hash", "kind", "safe_hast", "node_count"])
        || typeof value.node_count !== "number" || !Number.isSafeInteger(value.node_count)) {
      return { valid: false, end, node_count: 0, mermaid: false };
    }
    const tree = validate_safe_hast(value.safe_hast);
    return { valid: tree.valid && tree.node_count === value.node_count, end, node_count: tree.node_count, mermaid: false };
  }
  if (value.kind === "mermaid") {
    const valid = has_exact_keys(value, ["block_id", "source_span", "content_hash", "kind", "mermaid_source"])
      && typeof value.mermaid_source === "string"
      && utf8_byte_length(value.mermaid_source) <= MAXIMUM_MERMAID_BLOCK_BYTES;
    return { valid, end, node_count: 0, mermaid: true };
  }
  return { valid: false, end, node_count: 0, mermaid: false };
}

export function is_preview_document(value: unknown): value is preview_document {
  if (!is_record(value)
      || !has_exact_keys(value, ["protocol_version", "document_id", "revision", "source_length", "source_byte_length", "blocks", "diagnostics", "node_count"])
      || value.protocol_version !== PREVIEW_PROTOCOL_VERSION
      || !is_document_id(value.document_id) || !is_revision(value.revision)
      || typeof value.source_length !== "number" || !Number.isSafeInteger(value.source_length) || value.source_length < 0
      || typeof value.source_byte_length !== "number" || !Number.isSafeInteger(value.source_byte_length)
      || value.source_byte_length < 0 || value.source_byte_length > MAXIMUM_PREVIEW_SOURCE_BYTES
      || typeof value.node_count !== "number" || !Number.isSafeInteger(value.node_count)
      || !Array.isArray(value.blocks) || value.blocks.length > MAXIMUM_PREVIEW_BLOCKS
      || !Array.isArray(value.diagnostics) || value.diagnostics.length > MAXIMUM_PREVIEW_DIAGNOSTICS
      || !value.diagnostics.every(is_preview_diagnostic)) return false;

  let previous_end = 0;
  let node_count = 0;
  let mermaid_count = 0;
  for (const block of value.blocks) {
    const validated = validate_preview_block(block, value.source_length, previous_end);
    if (!validated.valid) return false;
    previous_end = validated.end;
    node_count += validated.node_count;
    if (validated.mermaid) mermaid_count += 1;
  }
  if (node_count !== value.node_count || node_count > MAXIMUM_SAFE_HAST_NODES
      || mermaid_count > MAXIMUM_MERMAID_BLOCKS) return false;
  try {
    return utf8_byte_length(JSON.stringify(value.blocks)) <= MAXIMUM_SAFE_HAST_BYTES;
  } catch {
    return false;
  }
}

export function is_preview_worker_response(value: unknown): value is preview_worker_response {
  if (!is_record(value) || typeof value.message_type !== "string") return false;
  if (value.message_type === "preview_result") {
    return has_exact_keys(value, ["message_type", "document"]) && is_preview_document(value.document);
  }
  return value.message_type === "preview_failure"
    && has_exact_keys(value, ["message_type", "protocol_version", "document_id", "revision", "error_code", "user_message"])
    && value.protocol_version === PREVIEW_PROTOCOL_VERSION
    && is_document_id(value.document_id) && is_revision(value.revision)
    && (value.error_code === "INVALID_PREVIEW_REQUEST" || value.error_code === "PREVIEW_SOURCE_TOO_LARGE"
      || value.error_code === "PREVIEW_TREE_TOO_DEEP" || value.error_code === "PREVIEW_TREE_TOO_LARGE"
      || value.error_code === "PREVIEW_PARSE_FAILED")
    && typeof value.user_message === "string" && value.user_message.length > 0 && value.user_message.length <= 256;
}
