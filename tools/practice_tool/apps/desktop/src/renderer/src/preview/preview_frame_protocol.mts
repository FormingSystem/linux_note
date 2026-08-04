import {
  MAXIMUM_MERMAID_BLOCK_BYTES,
  PREVIEW_PROTOCOL_VERSION,
  type preview_mermaid_block,
} from "@loop/markdown-engine/contracts";
import { is_workbench_theme, type workbench_theme } from "../theme/workbench_theme.mts";

const SESSION_NONCE_PATTERN = /^[a-f0-9]{32}$/u;
const IDENTIFIER_PATTERN = /^[a-zA-Z0-9_-]{8,160}$/u;
const CONTENT_HASH_PATTERN = /^[a-f0-9]{16}$/u;
const MAXIMUM_FRAME_HEIGHT = 10_000;

export interface mermaid_connect_message {
  message_type: "mermaid_connect";
  protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
  session_nonce: string;
}

export interface mermaid_render_message {
  message_type: "render_mermaid";
  protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
  session_nonce: string;
  document_id: string;
  revision: number;
  theme: workbench_theme;
  block: preview_mermaid_block;
}

export type mermaid_frame_message =
  | {
    message_type: "mermaid_ready";
    protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
    session_nonce: string;
  }
  | {
    message_type: "mermaid_rendered";
    protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
    session_nonce: string;
    document_id: string;
    block_id: string;
    revision: number;
    height: number;
  }
  | {
    message_type: "mermaid_render_error";
    protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
    session_nonce: string;
    document_id: string;
    block_id: string;
    revision: number;
    error_code: "INVALID_MERMAID_MESSAGE" | "MERMAID_DOCUMENT_CONFIG_REJECTED" | "MERMAID_RENDER_FAILED" | "UNSAFE_MERMAID_SVG";
    detail: string;
  }
  | {
    message_type: "mermaid_activate" | "mermaid_save_requested" | "mermaid_source_mode_requested";
    protocol_version: typeof PREVIEW_PROTOCOL_VERSION;
    session_nonce: string;
    document_id: string;
    block_id: string;
    revision: number;
  };

function is_record(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function has_exact_keys(value: Record<string, unknown>, keys: readonly string[]): boolean {
  const actual = Object.keys(value);
  return actual.length === keys.length && keys.every((key) => Object.hasOwn(value, key));
}

function is_identifier(value: unknown): value is string {
  return typeof value === "string" && IDENTIFIER_PATTERN.test(value);
}

function is_revision(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value >= 0;
}

function is_nonce(value: unknown): value is string {
  return typeof value === "string" && SESSION_NONCE_PATTERN.test(value);
}

function utf8_byte_length(value: string): number {
  return new TextEncoder().encode(value).byteLength;
}

function is_mermaid_block(value: unknown): value is preview_mermaid_block {
  return is_record(value)
    && has_exact_keys(value, ["block_id", "source_span", "content_hash", "kind", "mermaid_source"])
    && is_identifier(value.block_id)
    && value.kind === "mermaid"
    && typeof value.content_hash === "string" && CONTENT_HASH_PATTERN.test(value.content_hash)
    && is_record(value.source_span) && has_exact_keys(value.source_span, ["start", "end"])
    && typeof value.source_span.start === "number" && Number.isSafeInteger(value.source_span.start)
    && typeof value.source_span.end === "number" && Number.isSafeInteger(value.source_span.end)
    && value.source_span.start >= 0 && value.source_span.end > value.source_span.start
    && typeof value.mermaid_source === "string"
    && utf8_byte_length(value.mermaid_source) <= MAXIMUM_MERMAID_BLOCK_BYTES;
}

export function create_mermaid_connect_message(session_nonce: string): mermaid_connect_message {
  if (!is_nonce(session_nonce)) throw new Error("Mermaid Frame 会话 nonce 无效");
  return { message_type: "mermaid_connect", protocol_version: PREVIEW_PROTOCOL_VERSION, session_nonce };
}

export function create_mermaid_render_message(
  session_nonce: string,
  document_id: string,
  revision: number,
  block: preview_mermaid_block,
  theme: workbench_theme,
): mermaid_render_message {
  const value: mermaid_render_message = {
    message_type: "render_mermaid",
    protocol_version: PREVIEW_PROTOCOL_VERSION,
    session_nonce,
    document_id,
    revision,
    theme,
    block,
  };
  if (!is_mermaid_render_message(value, session_nonce)) throw new Error("Mermaid 渲染消息无效");
  return value;
}

export function is_mermaid_connect_message(value: unknown): value is mermaid_connect_message {
  return is_record(value)
    && has_exact_keys(value, ["message_type", "protocol_version", "session_nonce"])
    && value.message_type === "mermaid_connect"
    && value.protocol_version === PREVIEW_PROTOCOL_VERSION
    && is_nonce(value.session_nonce);
}

export function is_mermaid_render_message(value: unknown, expected_nonce: string): value is mermaid_render_message {
  return is_record(value)
    && has_exact_keys(value, ["message_type", "protocol_version", "session_nonce", "document_id", "revision", "theme", "block"])
    && value.message_type === "render_mermaid"
    && value.protocol_version === PREVIEW_PROTOCOL_VERSION
    && value.session_nonce === expected_nonce
    && is_identifier(value.document_id)
    && is_revision(value.revision)
    && is_workbench_theme(value.theme)
    && is_mermaid_block(value.block);
}

export function is_mermaid_frame_message(value: unknown, expected_nonce: string): value is mermaid_frame_message {
  if (!is_record(value) || value.protocol_version !== PREVIEW_PROTOCOL_VERSION
      || value.session_nonce !== expected_nonce || !is_nonce(value.session_nonce)
      || typeof value.message_type !== "string") return false;
  if (value.message_type === "mermaid_ready") {
    return has_exact_keys(value, ["message_type", "protocol_version", "session_nonce"]);
  }
  const common = has_exact_keys(value, value.message_type === "mermaid_rendered"
    ? ["message_type", "protocol_version", "session_nonce", "document_id", "block_id", "revision", "height"]
    : value.message_type === "mermaid_render_error"
      ? ["message_type", "protocol_version", "session_nonce", "document_id", "block_id", "revision", "error_code", "detail"]
      : ["message_type", "protocol_version", "session_nonce", "document_id", "block_id", "revision"])
    && is_identifier(value.document_id) && is_identifier(value.block_id) && is_revision(value.revision);
  if (!common) return false;
  if (value.message_type === "mermaid_rendered") {
    return typeof value.height === "number" && Number.isSafeInteger(value.height)
      && value.height > 0 && value.height <= MAXIMUM_FRAME_HEIGHT;
  }
  if (value.message_type === "mermaid_render_error") {
    return (value.error_code === "INVALID_MERMAID_MESSAGE" || value.error_code === "MERMAID_DOCUMENT_CONFIG_REJECTED"
      || value.error_code === "MERMAID_RENDER_FAILED"
      || value.error_code === "UNSAFE_MERMAID_SVG")
      && typeof value.detail === "string" && /^[a-z0-9:_-]{1,96}$/u.test(value.detail);
  }
  return value.message_type === "mermaid_activate" || value.message_type === "mermaid_save_requested"
    || value.message_type === "mermaid_source_mode_requested";
}
