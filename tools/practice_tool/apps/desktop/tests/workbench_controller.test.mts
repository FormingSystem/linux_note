import assert from "node:assert/strict";
import test from "node:test";
import type { BrowserWindow } from "electron";
import type {
  native_method,
  native_request_params_by_method,
  native_response_envelope,
  native_service_info,
} from "@loop/ipc-contracts";
import {
  workbench_controller,
  type dialog_port,
  type native_service_client,
} from "../src/main/workbench/workbench_controller.mts";

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
  readonly calls: Array<{ method: native_method; params: unknown }> = [];
  readonly responses: native_response_envelope[] = [];

  snapshot(): native_service_info {
    return {
      status: "ready",
      protocol_version: 2,
      service_version: "0.2.0",
      message: "ready",
    };
  }

  async request<method_type extends native_method>(
    method: method_type,
    params: native_request_params_by_method[method_type],
  ): Promise<native_response_envelope> {
    this.calls.push({ method, params });
    const response = this.responses.shift();
    if (!response) throw new Error("missing fake response");
    return response;
  }
}

function success(result: unknown, request_id = "test-request"): native_response_envelope {
  return { protocol_version: 2, request_id, ok: true, result };
}

function not_found(): native_response_envelope {
  return {
    protocol_version: 2,
    request_id: "not-found",
    ok: false,
    error: {
      code: "NOT_FOUND",
      user_message: "目标不存在",
      retryable: true,
      recovery_actions: ["RETRY", "CHOOSE_ANOTHER"],
      correlation_id: "not-found",
    },
  };
}

const fake_owner = {} as BrowserWindow;

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
