import { randomUUID } from "node:crypto";
// `.mts` keeps this dependency-free controller directly testable as an ES module.
import type { BrowserWindow } from "electron";
import {
  is_entry_page,
  is_list_children_request,
  is_native_close_result,
  is_opened_folder,
  is_opened_single_file,
  native_error_to_desktop_error,
  type command_result,
  type desktop_error,
  type entry_page,
  type list_children_request,
  type native_method,
  type native_request_params_by_method,
  type native_response_envelope,
  type native_service_info,
  type opened_folder,
  type opened_single_file,
} from "@loop/ipc-contracts";

type window_context = {
  window_session_id: string;
  workspace_id: string | null;
  operation_revision: number;
  open_pending: boolean;
};

export interface dialog_port {
  choose_markdown(owner: BrowserWindow): Promise<string | null>;
  choose_folder(owner: BrowserWindow): Promise<string | null>;
}

export interface native_service_client {
  snapshot(): native_service_info;
  request<method_type extends native_method>(
    method: method_type,
    params: native_request_params_by_method[method_type],
    timeout_ms?: number,
  ): Promise<native_response_envelope>;
}

function unavailable_error(message = "本地文件服务当前不可用"): desktop_error {
  return {
    code: "NATIVE_UNAVAILABLE",
    user_message: message,
    retryable: true,
    recovery_actions: ["RETRY"],
    correlation_id: randomUUID(),
  };
}

function invalid_request_error(message: string): desktop_error {
  return {
    code: "INVALID_REQUEST",
    user_message: message,
    retryable: false,
    recovery_actions: [],
    correlation_id: randomUUID(),
  };
}

export class workbench_controller {
  private readonly contexts = new Map<number, window_context>();
  private readonly native_service: native_service_client;
  private readonly dialogs: dialog_port;

  constructor(
    native_service: native_service_client,
    dialogs: dialog_port,
  ) {
    this.native_service = native_service;
    this.dialogs = dialogs;
  }

  register_window(web_contents_id: number): void {
    this.contexts.set(web_contents_id, {
      window_session_id: `window_${randomUUID().replaceAll("-", "")}`,
      workspace_id: null,
      operation_revision: 0,
      open_pending: false,
    });
  }

  unregister_window(web_contents_id: number): void {
    const context = this.contexts.get(web_contents_id);
    this.contexts.delete(web_contents_id);
    if (!context || this.native_service.snapshot().status !== "ready") return;
    void this.native_service.request("workspace.close", {
      window_session_id: context.window_session_id,
    }).catch(() => undefined);
  }

  async open_file(web_contents_id: number, owner: BrowserWindow): Promise<command_result<opened_single_file>> {
    const context = this.contexts.get(web_contents_id);
    if (!context) return { status: "error", error: invalid_request_error("窗口会话已经失效") };
    if (context.open_pending) return { status: "error", error: invalid_request_error("已有打开对话框正在处理") };
    context.open_pending = true;
    const revision = ++context.operation_revision;

    try {
      const locator = await this.dialogs.choose_markdown(owner);
      if (!locator) return { status: "cancelled" };
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      const response = await this.native_service.request("workspace.open_file", {
        window_session_id: context.window_session_id,
        locator,
      });
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      if (!response.ok) return { status: "error", error: native_error_to_desktop_error(response.error) };
      if (!is_opened_single_file(response.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效文件信息") };
      }
      context.workspace_id = response.result.workspace_id;
      return { status: "ok", value: response.result };
    } catch {
      return { status: "error", error: unavailable_error() };
    } finally {
      context.open_pending = false;
    }
  }

  async open_folder(web_contents_id: number, owner: BrowserWindow): Promise<command_result<opened_folder>> {
    const context = this.contexts.get(web_contents_id);
    if (!context) return { status: "error", error: invalid_request_error("窗口会话已经失效") };
    if (context.open_pending) return { status: "error", error: invalid_request_error("已有打开对话框正在处理") };
    context.open_pending = true;
    const revision = ++context.operation_revision;

    try {
      const locator = await this.dialogs.choose_folder(owner);
      if (!locator) return { status: "cancelled" };
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      const response = await this.native_service.request("workspace.open_folder", {
        window_session_id: context.window_session_id,
        locator,
      });
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      if (!response.ok) return { status: "error", error: native_error_to_desktop_error(response.error) };
      if (!is_opened_folder(response.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效文件夹信息") };
      }
      context.workspace_id = response.result.workspace_id;
      return { status: "ok", value: response.result };
    } catch {
      return { status: "error", error: unavailable_error() };
    } finally {
      context.open_pending = false;
    }
  }

  async close_workspace(web_contents_id: number): Promise<command_result<void>> {
    const context = this.contexts.get(web_contents_id);
    if (!context) return { status: "error", error: invalid_request_error("窗口会话已经失效") };
    ++context.operation_revision;
    try {
      const response = await this.native_service.request("workspace.close", {
        window_session_id: context.window_session_id,
      });
      if (!response.ok) return { status: "error", error: native_error_to_desktop_error(response.error) };
      if (!is_native_close_result(response.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效关闭状态") };
      }
      context.workspace_id = null;
      return { status: "ok", value: undefined };
    } catch {
      return { status: "error", error: unavailable_error() };
    }
  }

  private is_current_operation(
    web_contents_id: number,
    context: window_context,
    revision: number,
  ): boolean {
    return this.contexts.get(web_contents_id) === context && context.operation_revision === revision;
  }

  async list_children(web_contents_id: number, value: unknown): Promise<command_result<entry_page>> {
    if (!is_list_children_request(value)) {
      return { status: "error", error: invalid_request_error("目录枚举请求无效") };
    }
    const request: list_children_request = value;
    const context = this.contexts.get(web_contents_id);
    if (!context || context.workspace_id !== request.workspace_id) {
      return { status: "error", error: invalid_request_error("目录不属于当前窗口工作区") };
    }

    try {
      const params = request.cursor === undefined
        ? {
            window_session_id: context.window_session_id,
            workspace_id: request.workspace_id,
            directory_id: request.directory_id,
          }
        : {
            window_session_id: context.window_session_id,
            workspace_id: request.workspace_id,
            directory_id: request.directory_id,
            cursor: request.cursor,
          };
      const response = await this.native_service.request("workspace.list_children", params);
      if (this.contexts.get(web_contents_id) !== context || context.workspace_id !== request.workspace_id) {
        return { status: "error", error: invalid_request_error("目录能力已经被新的工作区替换") };
      }
      if (!response.ok) return { status: "error", error: native_error_to_desktop_error(response.error) };
      if (!is_entry_page(response.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效目录信息") };
      }
      return { status: "ok", value: response.result };
    } catch {
      return { status: "error", error: unavailable_error() };
    }
  }
}
