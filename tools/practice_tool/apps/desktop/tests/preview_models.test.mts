import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import {
  PREVIEW_PROTOCOL_VERSION,
  parse_markdown_preview,
  type preview_document,
  type preview_mermaid_block,
  type preview_source_request,
} from "@loop/markdown-engine";
import {
  preview_asset_name,
  PREVIEW_CONTENT_SECURITY_POLICY,
  PREVIEW_SCHEME_PRIVILEGES,
} from "../src/main/protocols/preview_protocol_policy.mts";
import { source_span_is_active } from "../src/renderer/src/editor/live_markdown_model.mts";
import { mermaid_source_has_document_configuration } from "../src/renderer/src/preview/mermaid_source_policy.mts";
import { preview_coordinator, type preview_worker_port } from "../src/renderer/src/preview/preview_coordinator.mts";
import {
  create_mermaid_connect_message,
  create_mermaid_render_message,
  is_mermaid_frame_message,
  is_mermaid_render_message,
} from "../src/renderer/src/preview/preview_frame_protocol.mts";

const document_id = "0123456789abcdef0123456789abcdef";
const session_nonce = "0123456789abcdef0123456789abcdef";

function request(revision: number, source = `# 修订 ${revision}`): preview_source_request {
  return { message_type: "parse_preview", protocol_version: PREVIEW_PROTOCOL_VERSION, document_id, revision, source };
}

class fake_worker implements preview_worker_port {
  readonly posted: unknown[] = [];
  terminated = false;
  readonly #message_listeners = new Set<(event: MessageEvent<unknown>) => void>();
  readonly #error_listeners = new Set<(event: Event) => void>();
  readonly #message_error_listeners = new Set<(event: Event) => void>();

  postMessage(message: unknown): void { this.posted.push(message); }
  terminate(): void { this.terminated = true; }

  addEventListener(type: "message" | "error" | "messageerror", listener: ((event: MessageEvent<unknown>) => void) | ((event: Event) => void)): void {
    if (type === "message") this.#message_listeners.add(listener as (event: MessageEvent<unknown>) => void);
    else if (type === "error") this.#error_listeners.add(listener as (event: Event) => void);
    else this.#message_error_listeners.add(listener as (event: Event) => void);
  }

  removeEventListener(type: "message" | "error" | "messageerror", listener: ((event: MessageEvent<unknown>) => void) | ((event: Event) => void)): void {
    if (type === "message") this.#message_listeners.delete(listener as (event: MessageEvent<unknown>) => void);
    else if (type === "error") this.#error_listeners.delete(listener as (event: Event) => void);
    else this.#message_error_listeners.delete(listener as (event: Event) => void);
  }

  emit_message(data: unknown): void {
    for (const listener of this.#message_listeners) listener(new MessageEvent("message", { data }));
  }

  emit_error(): void {
    for (const listener of this.#error_listeners) listener(new Event("error"));
  }
}

test("预览协调器只保留一个在途任务和最新待处理修订", () => {
  const workers: fake_worker[] = [];
  const results: preview_document[] = [];
  const coordinator = new preview_coordinator(
    () => {
      const worker = new fake_worker();
      workers.push(worker);
      return worker;
    },
    { on_result: (document) => results.push(document), on_failure: () => undefined },
  );
  coordinator.submit(request(1));
  coordinator.submit(request(2));
  coordinator.submit(request(3));
  assert.deepEqual(workers[0]?.posted, [request(1)]);
  workers[0]?.emit_message({ message_type: "preview_result", document: parse_markdown_preview(request(1)) });
  assert.equal(results.length, 0);
  assert.deepEqual(workers[0]?.posted, [request(1), request(3)]);
  workers[0]?.emit_message({ message_type: "preview_result", document: parse_markdown_preview(request(3)) });
  assert.deepEqual(results.map((item) => item.revision), [3]);
  coordinator.dispose();
});

test("预览协调器拒绝超限源码且不投递给 Worker", () => {
  const worker = new fake_worker();
  const failures: string[] = [];
  const coordinator = new preview_coordinator(
    () => worker,
    { on_result: () => undefined, on_failure: (failure) => failures.push(failure.error_code) },
  );
  coordinator.submit(request(1, "a".repeat(5 * 1024 * 1024 + 1)));
  assert.equal(worker.posted.length, 0);
  assert.deepEqual(failures, ["PREVIEW_SOURCE_TOO_LARGE"]);
  coordinator.dispose();
});

test("无效 Worker 响应会终止并重建隔离 Worker", () => {
  const workers: fake_worker[] = [];
  const failures: string[] = [];
  const coordinator = new preview_coordinator(
    () => {
      const worker = new fake_worker();
      workers.push(worker);
      return worker;
    },
    { on_result: () => undefined, on_failure: (failure) => failures.push(failure.user_message) },
  );
  coordinator.submit(request(1));
  workers[0]?.emit_message({ message_type: "unknown" });
  assert.equal(workers[0]?.terminated, true);
  assert.equal(workers.length, 2);
  assert.deepEqual(failures, ["预览 Worker 返回了无效消息"]);
  workers[1]?.emit_error();
  assert.equal(workers[1]?.terminated, true);
  assert.equal(workers.length, 2, "空闲重建 Worker 再次失败时不得形成重启循环");
  coordinator.submit(request(2));
  assert.equal(workers.length, 3, "新任务到来时才重新建立 Worker");
  coordinator.dispose();
});

test("Mermaid Frame 消息协议严格绑定版本、nonce、块与修订", () => {
  const document = parse_markdown_preview(request(4, "```mermaid\nflowchart TD\nA --> B\n```"));
  const block = document.blocks[0] as preview_mermaid_block;
  assert.deepEqual(create_mermaid_connect_message(session_nonce), {
    message_type: "mermaid_connect",
    protocol_version: 3,
    session_nonce,
  });
  const render_message = create_mermaid_render_message(session_nonce, document_id, 4, block, "dark");
  assert.equal(render_message.block, block);
  assert.equal(render_message.theme, "dark");
  assert.equal(is_mermaid_render_message({ ...render_message, theme: "light" }, session_nonce), true);
  assert.equal(is_mermaid_render_message({ ...render_message, protocol_version: 2 }, session_nonce), false);
  assert.equal(is_mermaid_render_message({ ...render_message, theme: "green" }, session_nonce), false);
  assert.equal(is_mermaid_render_message({ ...render_message, legacy_theme: "dark" }, session_nonce), false);
  assert.equal(is_mermaid_frame_message({
    message_type: "mermaid_rendered",
    protocol_version: 3,
    session_nonce,
    document_id,
    block_id: block.block_id,
    revision: 4,
    height: 240,
  }, session_nonce), true);
  assert.equal(is_mermaid_frame_message({
    message_type: "mermaid_rendered",
    protocol_version: 3,
    session_nonce,
    document_id,
    block_id: block.block_id,
    revision: 4,
    height: 240,
    command: "save",
  }, session_nonce), false);
  assert.equal(is_mermaid_frame_message({
    message_type: "mermaid_ready",
    protocol_version: 3,
    session_nonce: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  }, session_nonce), false);
  assert.equal(is_mermaid_frame_message({
    message_type: "mermaid_source_mode_requested",
    protocol_version: 3,
    session_nonce,
    document_id,
    block_id: block.block_id,
    revision: 4,
  }, session_nonce), true);
  assert.equal(is_mermaid_frame_message({
    message_type: "mermaid_render_error",
    protocol_version: 3,
    session_nonce,
    document_id,
    block_id: block.block_id,
    revision: 4,
    error_code: "UNSAFE_MERMAID_SVG",
    detail: "attribute:data-id",
  }, session_nonce), true);
  assert.equal(is_mermaid_frame_message({
    message_type: "mermaid_render_error",
    protocol_version: 3,
    session_nonce,
    document_id,
    block_id: block.block_id,
    revision: 4,
    error_code: "MERMAID_DOCUMENT_CONFIG_REJECTED",
    detail: "document_configuration",
  }, session_nonce), true);
});

test("Mermaid 文档配置不能覆盖工作台主题或 runtime", () => {
  assert.equal(mermaid_source_has_document_configuration("flowchart TD\nA --> B"), false);
  assert.equal(mermaid_source_has_document_configuration("%% 普通注释\nflowchart TD\nA --> B"), false);
  assert.equal(mermaid_source_has_document_configuration("%%{init: { 'theme': 'forest' }}%%\nflowchart TD\nA --> B"), true);
  assert.equal(mermaid_source_has_document_configuration("%% { config: { themeCSS: 'button { display: none }' } }%%\nflowchart TD"), true);
  assert.equal(mermaid_source_has_document_configuration("---\ntitle: 安全标题\n---\nflowchart TD"), true);
  assert.equal(mermaid_source_has_document_configuration("---\nconfig:\n  theme: forest\n---\nflowchart TD"), true);
  assert.equal(mermaid_source_has_document_configuration('---\n"config":\n  fontSize: 512\n---\nflowchart TD'), true);
  assert.equal(mermaid_source_has_document_configuration("---\n'config':\n  theme: forest\n---\nflowchart TD"), true);
  assert.equal(mermaid_source_has_document_configuration('---\n"con\\u0066ig":\n  logLevel: debug\n---\nflowchart TD'), true);
});

test("混合编辑只让当前选择相交块显示源码", () => {
  assert.equal(source_span_is_active({ start: 10, end: 20 }, [{ from: 12, to: 12 }]), true);
  assert.equal(source_span_is_active({ start: 10, end: 20 }, [{ from: 0, to: 9 }]), false);
  assert.equal(source_span_is_active({ start: 10, end: 20 }, [{ from: 20, to: 20 }]), true);
});

test("loop-preview 协议只允许固定 Mermaid runtime 资源并禁用网络", () => {
  assert.equal(preview_asset_name("loop-preview://preview/"), "index.html");
  assert.equal(preview_asset_name("loop-preview://preview/runtime.js"), "runtime.js");
  assert.equal(preview_asset_name("loop-preview://preview/styles.css"), "styles.css");
  assert.equal(preview_asset_name("loop-preview://preview/../index.html"), null);
  assert.equal(preview_asset_name("loop-preview://preview/%2e%2e/secret"), null);
  assert.equal(preview_asset_name("loop-preview://other/runtime.js"), null);
  assert.equal(preview_asset_name("loop-preview://preview/runtime.js?x=1"), null);
  assert.match(PREVIEW_CONTENT_SECURITY_POLICY, /connect-src 'none'/u);
  assert.match(PREVIEW_CONTENT_SECURITY_POLICY, /default-src 'none'/u);
  assert.equal(PREVIEW_SCHEME_PRIVILEGES.corsEnabled, true);
  assert.equal(Object.hasOwn(PREVIEW_SCHEME_PRIVILEGES, "supportFetch"), false);
  assert.equal(Object.hasOwn(PREVIEW_SCHEME_PRIVILEGES, "bypassCSP"), false);
});

test("Mermaid runtime 固定 strict、安全 SVG allowlist 且不直接写 innerHTML", async () => {
  const runtime_path = new URL("../src/renderer/src/preview_frame/runtime.ts", import.meta.url);
  const source = await readFile(runtime_path, "utf8");
  assert.match(source, /securityLevel: "strict"/u);
  assert.match(source, /htmlLabels: false/u);
  assert.match(source, /SAFE_SVG_ELEMENTS/u);
  assert.match(source, /MAXIMUM_MERMAID_SVG_BYTES/u);
  assert.match(source, /MAXIMUM_MERMAID_SVG_ELEMENTS/u);
  assert.match(source, /MAXIMUM_MERMAID_SVG_DEPTH/u);
  assert.match(source, /MINIMUM_ZOOM = 0\.5/u);
  assert.match(source, /MAXIMUM_ZOOM = 3/u);
  assert.match(source, /ZOOM_STEP = 0\.25/u);
  assert.match(source, /data-mermaid-action/u);
  assert.match(source, /"themeCSS"/u);
  assert.match(source, /mermaid_source_has_document_configuration/u);
  assert.equal(source.includes('"foreignobject"'), false);
  assert.equal(source.includes("innerHTML"), false);
  assert.equal(source.includes("fetch("), false);
});

test("非活动标签销毁混合预览装饰而保留 CodeMirror 会话", async () => {
  const surface_source = await readFile(new URL("../src/renderer/src/editor/live_markdown_surface.ts", import.meta.url), "utf8");
  const editor_source = await readFile(new URL("../src/renderer/src/editor/document_editor.tsx", import.meta.url), "utf8");
  assert.match(surface_source, /if \(!state\.active \|\| state\.source_mode\) return Decoration\.none/u);
  assert.match(editor_source, /set_live_active\(view, properties\.active\)/u);
});
