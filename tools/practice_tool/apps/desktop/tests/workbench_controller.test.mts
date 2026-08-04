import assert from "node:assert/strict";
import test from "node:test";
import type { BrowserWindow } from "electron";
import type {
  native_method,
  native_request_params_by_method,
  native_response_envelope,
  native_service_info,
} from "@loop/ipc-contracts";
import { NATIVE_PROTOCOL_VERSION } from "@loop/ipc-contracts";
import {
  workbench_controller,
  type dialog_port,
  type native_service_client,
} from "../src/main/workbench/workbench_controller.mts";
import { requires_close_confirmation } from "../src/main/workbench/window_close_policy.mts";

const workspace_id = "workspace_0123456789abcdef0123456789abcdef";
const directory_id = "directory_0123456789abcdef0123456789abcdef";

class fake_dialogs implements dialog_port {
  markdown: string | null = null;
  folder: string | null = null;

  async choose_markdown(_owner: BrowserWindow): Promise<string | null> {
    return this.markdown;
  }

  async choose_folder(_owner: BrowserWindow): Promise<string | null> {
    return this.folder;
  }
}

class fake_native implements native_service_client {
  readonly calls: Array<{ method: native_method; params: unknown; options?: { body?: Buffer; timeout_ms?: number } }> = [];
  readonly responses: Array<
    { envelope: native_response_envelope; body: Buffer }
    | Promise<{ envelope: native_response_envelope; body: Buffer }>
  > = [];

  snapshot(): native_service_info {
    return {
      status: "ready",
      protocol_version: NATIVE_PROTOCOL_VERSION,
      service_version: "0.4.0",
      message: "ready",
    };
  }

  async request<method_type extends native_method>(
    method: method_type,
    params: native_request_params_by_method[method_type],
    options?: { body?: Buffer; timeout_ms?: number },
  ): Promise<{ envelope: native_response_envelope; body: Buffer }> {
    this.calls.push(options === undefined ? { method, params } : { method, params, options });
    const response = await this.responses.shift();
    if (!response) throw new Error("missing fake response");
    return response;
  }
}

function success(result: unknown, request_id = "test-request") {
  return {
    envelope: { protocol_version: NATIVE_PROTOCOL_VERSION, request_id, ok: true, body: null, result } satisfies native_response_envelope,
    body: Buffer.alloc(0),
  };
}

function not_found() {
  return {
    envelope: {
      protocol_version: NATIVE_PROTOCOL_VERSION,
      request_id: "not-found",
      ok: false,
      body: null,
      error: {
        code: "NOT_FOUND",
        user_message: "目标不存在",
        retryable: true,
        recovery_actions: ["RETRY", "CHOOSE_ANOTHER"],
        correlation_id: "not-found",
      },
    } satisfies native_response_envelope,
    body: Buffer.alloc(0),
  };
}

const fake_owner = {} as BrowserWindow;

test("Dirty 草稿在 Native 工作区已撤销后仍要求关闭确认", () => {
  assert.equal(requires_close_confirmation(false, 1), true);
  assert.equal(requires_close_confirmation(true, 0), true);
  assert.equal(requires_close_confirmation(false, 0), false);
});

test("取消系统对话框不调用 Native Service", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(1);

  assert.deepEqual(await controller.open_file(1, fake_owner), { status: "cancelled" });
  assert.equal(native.calls.length, 0);
});

test("打开失败保留原工作区且 Renderer 只能用不透明 ID 枚举", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  dialogs.folder = "C:/trusted";
  dialogs.markdown = "C:/missing.md";
  native.responses.push(
    success({
      workspace_id,
      mode: "folder",
      display_name: "trusted",
      root_directory_id: directory_id,
      resolved_from_link: false,
    }),
    not_found(),
    success({
      workspace_id,
      directory_id,
      entries: [],
      next_cursor: null,
      total_entries: 0,
    }),
  );

  const controller = new workbench_controller(native, dialogs);
  controller.register_window(2);
  assert.equal((await controller.open_folder(2, fake_owner)).status, "ok");
  assert.equal((await controller.open_file(2, fake_owner)).status, "error");
  assert.equal((await controller.list_children(2, { workspace_id, directory_id })).status, "ok");

  assert.equal(native.calls[0]?.method, "workspace.open_folder");
  assert.equal((native.calls[0]?.params as { locator?: string }).locator, "C:/trusted");
  assert.equal(native.calls[2]?.method, "workspace.list_children");
  assert.equal(Object.hasOwn(native.calls[2]?.params as object, "locator"), false);
});

test("跨工作区目录 ID 在 Main 层失败关闭", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  dialogs.folder = "C:/trusted";
  native.responses.push(success({
    workspace_id,
    mode: "folder",
    display_name: "trusted",
    root_directory_id: directory_id,
    resolved_from_link: false,
  }));

  const controller = new workbench_controller(native, dialogs);
  controller.register_window(3);
  await controller.open_folder(3, fake_owner);
  const result = await controller.list_children(3, {
    workspace_id: "workspace_ffffffffffffffffffffffffffffffff",
    directory_id,
  });
  assert.equal(result.status, "error");
  assert.equal(native.calls.length, 1);
});

test("窗口注销会请求撤销 Native 能力", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  native.responses.push(success({ closed: true }));
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(4);
  controller.unregister_window(4);
  await new Promise((resolve) => setTimeout(resolve, 0));
  assert.equal(native.calls[0]?.method, "workspace.close");
});

test("关闭工作区会使尚未完成的系统对话框结果失效", async () => {
  const native = new fake_native();
  let finish_dialog: (value: string | null) => void = () => undefined;
  const dialogs: dialog_port = {
    choose_markdown: async () => new Promise<string | null>((resolve) => {
      finish_dialog = resolve;
    }),
    choose_folder: async () => null,
  };
  native.responses.push(success({ closed: true }));
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(5);

  const opening = controller.open_file(5, fake_owner);
  assert.equal((await controller.open_folder(5, fake_owner)).status, "error");
  assert.equal((await controller.close_workspace(5)).status, "ok");
  finish_dialog("C:/stale.md");

  assert.equal((await opening).status, "cancelled");
  assert.deepEqual(native.calls.map((call) => call.method), ["workspace.close"]);
});

test("系统对话框打开期间出现 Dirty 会在调用 Native 前取消替换", async () => {
  const native = new fake_native();
  let finish_dialog: (value: string | null) => void = () => undefined;
  const dialogs: dialog_port = {
    choose_markdown: async () => new Promise<string | null>((resolve) => {
      finish_dialog = resolve;
    }),
    choose_folder: async () => "C:/trusted",
  };
  native.responses.push(success({
    workspace_id,
    mode: "folder",
    display_name: "trusted",
    root_directory_id: directory_id,
    resolved_from_link: false,
  }));
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(9);
  assert.equal((await controller.open_folder(9, fake_owner)).status, "ok");
  assert.equal(controller.has_workspace(9), true);

  const replacement = controller.open_file(9, fake_owner);
  assert.equal(controller.report_dirty_state(9, { workspace_id, dirty_count: 1 }).status, "ok");
  finish_dialog("C:/replacement.md");
  const result = await replacement;
  assert.equal(result.status, "error");
  assert.deepEqual(native.calls.map((call) => call.method), ["workspace.open_folder"]);
});

test("Native 打开在途时出现 Dirty 会撤销新能力并保留内存草稿", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  dialogs.folder = "C:/trusted";
  dialogs.markdown = "C:/replacement.md";
  native.responses.push(success({
    workspace_id,
    mode: "folder",
    display_name: "trusted",
    root_directory_id: directory_id,
    resolved_from_link: false,
  }));
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(10);
  assert.equal((await controller.open_folder(10, fake_owner)).status, "ok");

  let finish_open: (value: ReturnType<typeof success>) => void = () => undefined;
  native.responses.push(new Promise((resolve) => {
    finish_open = resolve;
  }));
  native.responses.push(success({ closed: true }));
  const replacement = controller.open_file(10, fake_owner);
  await new Promise((resolve) => setTimeout(resolve, 0));
  assert.equal(controller.report_dirty_state(10, { workspace_id, dirty_count: 1 }).status, "ok");
  finish_open(success({
    workspace_id: "workspace_ffffffffffffffffffffffffffffffff",
    mode: "single_file",
    display_name: "replacement.md",
    document: {
      document_id: "document_ffffffffffffffffffffffffffffffff",
      name: "replacement.md",
      display_path: "replacement.md",
      byte_size: 0,
      encoding: "utf-8",
      bom: false,
      line_ending: "none",
      read_only: false,
      resolved_from_link: false,
    },
  }));

  const result = await replacement;
  assert.equal(result.status, "error");
  await new Promise((resolve) => setTimeout(resolve, 0));
  assert.deepEqual(native.calls.map((call) => call.method), [
    "workspace.open_folder",
    "workspace.open_file",
    "workspace.close",
  ]);
  assert.equal(controller.has_workspace(10), true);
  assert.equal(controller.dirty_count(10), 1);
  assert.equal(controller.report_dirty_state(10, { workspace_id, dirty_count: 0 }).status, "ok");
  native.responses.push(success({ closed: true }));
  assert.equal((await controller.close_workspace(10)).status, "ok");
  assert.equal(controller.has_workspace(10), false);
});

test("Native 关闭在途时出现 Dirty 不会把 Renderer 草稿伪装成已关闭", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  dialogs.folder = "C:/trusted";
  native.responses.push(success({
    workspace_id,
    mode: "folder",
    display_name: "trusted",
    root_directory_id: directory_id,
    resolved_from_link: false,
  }));
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(11);
  assert.equal((await controller.open_folder(11, fake_owner)).status, "ok");

  let finish_close: (value: ReturnType<typeof success>) => void = () => undefined;
  native.responses.push(new Promise((resolve) => {
    finish_close = resolve;
  }));
  const closing = controller.close_workspace(11);
  assert.equal(controller.report_dirty_state(11, { workspace_id, dirty_count: 1 }).status, "ok");
  finish_close(success({ closed: true }));

  const result = await closing;
  assert.equal(result.status, "error");
  assert.equal(controller.has_workspace(11), true);
  assert.equal(controller.dirty_count(11), 1);
  assert.equal(controller.report_dirty_state(11, { workspace_id, dirty_count: 0 }).status, "ok");
  native.responses.push(success({ closed: true }));
  assert.equal((await controller.close_workspace(11)).status, "ok");
  assert.equal(controller.has_workspace(11), false);
});

test("正文附件经窗口工作区绑定后进入 Renderer 且不包含绝对路径", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  dialogs.markdown = "C:/trusted/README.md";
  const document_id = "document_0123456789abcdef0123456789abcdef";
  const version_id = "version_0123456789abcdef0123456789abcdef";
  native.responses.push(
    success({
      workspace_id,
      mode: "single_file",
      display_name: "README.md",
      document: {
        document_id,
        name: "README.md",
        display_path: "README.md",
        byte_size: 5,
        encoding: "utf-8",
        bom: false,
        line_ending: "none",
        read_only: false,
        resolved_from_link: false,
      },
    }),
    {
      envelope: {
        protocol_version: NATIVE_PROTOCOL_VERSION,
        request_id: "document",
        ok: true,
        body: {
          kind: "markdown_utf8",
          byte_length: 5,
          sha256: "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
        },
        result: {
          workspace_id,
          document_id,
          name: "README.md",
          display_path: "README.md",
          content_hash: "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
          file_version_token: version_id,
          byte_size: 5,
          modified_time_ms: 1,
          encoding: "utf-8",
          bom: false,
          line_ending: "none",
          read_only: false,
          resolved_from_link: false,
        },
      },
      body: Buffer.from("hello"),
    },
  );
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(6);
  assert.equal((await controller.open_file(6, fake_owner)).status, "ok");
  const opened = await controller.open_document(6, { workspace_id, target_kind: "document", target_id: document_id });
  assert.equal(opened.status, "ok");
  if (opened.status === "ok") {
    assert.equal(opened.value.content, "hello");
    assert.equal(JSON.stringify(opened.value).includes("C:/trusted"), false);
  }
});

test("正文原始字节摘要与附件不一致时拒绝进入 Renderer", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  dialogs.markdown = "C:/trusted/README.md";
  const document_id = "document_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  native.responses.push(
    success({
      workspace_id,
      mode: "single_file",
      display_name: "README.md",
      document: {
        document_id,
        name: "README.md",
        display_path: "README.md",
        byte_size: 5,
        encoding: "utf-8",
        bom: false,
        line_ending: "none",
        read_only: false,
        resolved_from_link: false,
      },
    }),
    {
      envelope: {
        protocol_version: NATIVE_PROTOCOL_VERSION,
        request_id: "document-hash-mismatch",
        ok: true,
        body: {
          kind: "markdown_utf8",
          byte_length: 5,
          sha256: "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
        },
        result: {
          workspace_id,
          document_id,
          name: "README.md",
          display_path: "README.md",
          content_hash: "0".repeat(64),
          file_version_token: "version_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          byte_size: 5,
          modified_time_ms: 1,
          encoding: "utf-8",
          bom: false,
          line_ending: "none",
          read_only: false,
          resolved_from_link: false,
        },
      },
      body: Buffer.from("hello"),
    },
  );
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(8);
  assert.equal((await controller.open_file(8, fake_owner)).status, "ok");
  const opened = await controller.open_document(8, {
    workspace_id,
    target_kind: "document",
    target_id: document_id,
  });
  assert.equal(opened.status, "error");
  if (opened.status === "error") assert.match(opened.error.user_message, /摘要/);
});

test("保存正文只传不透明能力与二进制附件并严格验证响应", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  dialogs.markdown = "C:/trusted/README.md";
  const document_id = "document_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  const version_id = "version_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  const original_hash = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
  native.responses.push(
    success({
      workspace_id,
      mode: "single_file",
      display_name: "README.md",
      document: {
        document_id,
        name: "README.md",
        display_path: "README.md",
        byte_size: 5,
        encoding: "utf-8",
        bom: false,
        line_ending: "none",
        read_only: false,
        resolved_from_link: false,
      },
    }),
    success({
      workspace_id,
      document_id,
      content_hash: "27eb5e51506c911f6fc4bb345c0d9db6f60415fceab7c18e1e9b862637415777",
      file_version_token: "version_cccccccccccccccccccccccccccccccc",
      saved_revision: 7,
      byte_size: 7,
      modified_time_ms: 2,
      encoding: "utf-8",
      bom: false,
      line_ending: "none",
      read_only: false,
      resolved_from_link: false,
    }),
  );
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(12);
  assert.equal((await controller.open_file(12, fake_owner)).status, "ok");
  const saved = await controller.save_document(12, {
    workspace_id,
    document_id,
    expected_file_version_token: version_id,
    expected_content_hash: original_hash,
    editor_revision: 7,
    line_ending_policy: "preserve",
    content: "updated",
  });
  assert.equal(saved.status, "ok");
  assert.equal(native.calls[1]?.method, "workspace.save_document");
  assert.equal(native.calls[1]?.options?.body?.toString("utf8"), "updated");
  assert.equal(Object.hasOwn(native.calls[1]?.params as object, "content"), false);
  assert.equal(JSON.stringify(native.calls[1]).includes("C:/trusted"), false);

  const calls_before_invalid = native.calls.length;
  assert.equal((await controller.save_document(12, {
    workspace_id,
    document_id,
    expected_file_version_token: version_id,
    expected_content_hash: original_hash,
    editor_revision: 8,
    line_ending_policy: "preserve",
    content: "contains\0nul",
  })).status, "error");
  assert.equal(native.calls.length, calls_before_invalid);

  native.responses.push(success({ closed: true }));
  assert.equal((await controller.close_document(12, { workspace_id, document_id })).status, "ok");
  assert.equal(native.calls[2]?.method, "workspace.close_document");
  assert.equal(Object.hasOwn(native.calls[2]?.params as object, "path"), false);
});

test("Dirty 状态阻止工作区替换和关闭", async () => {
  const native = new fake_native();
  const dialogs = new fake_dialogs();
  dialogs.folder = "C:/trusted";
  native.responses.push(success({
    workspace_id,
    mode: "folder",
    display_name: "trusted",
    root_directory_id: directory_id,
    resolved_from_link: false,
  }));
  const controller = new workbench_controller(native, dialogs);
  controller.register_window(7);
  assert.equal((await controller.open_folder(7, fake_owner)).status, "ok");
  assert.equal(controller.report_dirty_state(7, { workspace_id, dirty_count: 1 }).status, "ok");
  assert.equal((await controller.close_workspace(7)).status, "error");
  assert.equal((await controller.open_file(7, fake_owner)).status, "error");
  assert.equal(controller.dirty_count(7), 1);
  assert.equal(native.calls.length, 1);
});
