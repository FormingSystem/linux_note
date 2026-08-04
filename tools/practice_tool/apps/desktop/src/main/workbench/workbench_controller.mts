import { createHash, randomUUID } from "node:crypto";
import { TextDecoder } from "node:util";
// `.mts` keeps this dependency-free controller directly testable as an ES module.
import type { BrowserWindow } from "electron";
import {
  is_entry_page,
  is_close_document_request,
  is_document_snapshot,
  is_list_children_request,
  is_native_document_snapshot,
  is_native_close_result,
  is_open_document_request,
  is_save_document_request,
  is_save_document_result,
  is_opened_folder,
  is_opened_single_file,
  is_report_dirty_state_request,
  native_error_to_desktop_error,
  type command_result,
  type desktop_error,
  type entry_page,
  type document_snapshot,
  type list_children_request,
  type native_method,
  type native_request_params_by_method,
  type native_response_envelope,
  type native_service_info,
  type opened_folder,
  type opened_single_file,
  type report_dirty_state_request,
  type save_document_result,
} from "@loop/ipc-contracts";

type window_context = {
  window_session_id: string;
  workspace_id: string | null;
  renderer_workspace_id: string | null;
  operation_revision: number;
  open_pending: boolean;
  dirty_count: number;
};

type native_service_response = {
  envelope: native_response_envelope;
  body: Buffer;
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
    options?: { body?: Buffer; timeout_ms?: number },
  ): Promise<native_service_response>;
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
      renderer_workspace_id: null,
      operation_revision: 0,
      open_pending: false,
      dirty_count: 0,
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
    if (context.dirty_count > 0) return { status: "error", error: invalid_request_error("请先放弃或处理内存中的未保存修改") };
    if (context.open_pending) return { status: "error", error: invalid_request_error("已有打开对话框正在处理") };
    context.open_pending = true;
    const revision = ++context.operation_revision;
    const previous_renderer_workspace_id = context.renderer_workspace_id;

    try {
      const locator = await this.dialogs.choose_markdown(owner);
      if (!locator) return { status: "cancelled" };
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      if (context.dirty_count > 0) {
        return { status: "error", error: invalid_request_error("打开期间出现未保存修改，已取消替换工作区") };
      }
      const response = await this.native_service.request("workspace.open_file", {
        window_session_id: context.window_session_id,
        locator,
      });
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      const envelope = response.envelope;
      if (!envelope.ok) return { status: "error", error: native_error_to_desktop_error(envelope.error) };
      if (!is_opened_single_file(envelope.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效文件信息") };
      }
      if (context.dirty_count > 0) {
        context.workspace_id = null;
        context.renderer_workspace_id = previous_renderer_workspace_id;
        void this.native_service.request("workspace.close", {
          window_session_id: context.window_session_id,
        }).catch(() => undefined);
        return { status: "error", error: invalid_request_error("本地文件服务响应前出现未保存修改；新能力已撤销，内存草稿仍保留") };
      }
      context.workspace_id = envelope.result.workspace_id;
      context.renderer_workspace_id = envelope.result.workspace_id;
      context.dirty_count = 0;
      return { status: "ok", value: envelope.result };
    } catch {
      return { status: "error", error: unavailable_error() };
    } finally {
      context.open_pending = false;
    }
  }

  async open_folder(web_contents_id: number, owner: BrowserWindow): Promise<command_result<opened_folder>> {
    const context = this.contexts.get(web_contents_id);
    if (!context) return { status: "error", error: invalid_request_error("窗口会话已经失效") };
    if (context.dirty_count > 0) return { status: "error", error: invalid_request_error("请先放弃或处理内存中的未保存修改") };
    if (context.open_pending) return { status: "error", error: invalid_request_error("已有打开对话框正在处理") };
    context.open_pending = true;
    const revision = ++context.operation_revision;
    const previous_renderer_workspace_id = context.renderer_workspace_id;

    try {
      const locator = await this.dialogs.choose_folder(owner);
      if (!locator) return { status: "cancelled" };
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      if (context.dirty_count > 0) {
        return { status: "error", error: invalid_request_error("打开期间出现未保存修改，已取消替换工作区") };
      }
      const response = await this.native_service.request("workspace.open_folder", {
        window_session_id: context.window_session_id,
        locator,
      });
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      const envelope = response.envelope;
      if (!envelope.ok) return { status: "error", error: native_error_to_desktop_error(envelope.error) };
      if (!is_opened_folder(envelope.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效文件夹信息") };
      }
      if (context.dirty_count > 0) {
        context.workspace_id = null;
        context.renderer_workspace_id = previous_renderer_workspace_id;
        void this.native_service.request("workspace.close", {
          window_session_id: context.window_session_id,
        }).catch(() => undefined);
        return { status: "error", error: invalid_request_error("本地文件服务响应前出现未保存修改；新能力已撤销，内存草稿仍保留") };
      }
      context.workspace_id = envelope.result.workspace_id;
      context.renderer_workspace_id = envelope.result.workspace_id;
      context.dirty_count = 0;
      return { status: "ok", value: envelope.result };
    } catch {
      return { status: "error", error: unavailable_error() };
    } finally {
      context.open_pending = false;
    }
  }

  async close_workspace(web_contents_id: number): Promise<command_result<void>> {
    const context = this.contexts.get(web_contents_id);
    if (!context) return { status: "error", error: invalid_request_error("窗口会话已经失效") };
    if (context.dirty_count > 0) return { status: "error", error: invalid_request_error("存在仅保存在内存中的修改，当前不能关闭工作区") };
    const revision = ++context.operation_revision;
    try {
      const response = await this.native_service.request("workspace.close", {
        window_session_id: context.window_session_id,
      });
      if (!this.is_current_operation(web_contents_id, context, revision)) return { status: "cancelled" };
      const envelope = response.envelope;
      if (!envelope.ok) return { status: "error", error: native_error_to_desktop_error(envelope.error) };
      if (!is_native_close_result(envelope.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效关闭状态") };
      }
      if (context.dirty_count > 0) {
        context.workspace_id = null;
        return { status: "error", error: invalid_request_error("关闭期间出现未保存修改；文件能力已经撤销，内存草稿仍保留") };
      }
      context.workspace_id = null;
      context.renderer_workspace_id = null;
      context.dirty_count = 0;
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
      const envelope = response.envelope;
      if (!envelope.ok) return { status: "error", error: native_error_to_desktop_error(envelope.error) };
      if (!is_entry_page(envelope.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效目录信息") };
      }
      return { status: "ok", value: envelope.result };
    } catch {
      return { status: "error", error: unavailable_error() };
    }
  }

  report_dirty_state(web_contents_id: number, value: unknown): command_result<void> {
    if (!is_report_dirty_state_request(value)) {
      return { status: "error", error: invalid_request_error("Dirty 状态请求无效") };
    }
    const request: report_dirty_state_request = value;
    const context = this.contexts.get(web_contents_id);
    if (!context || context.renderer_workspace_id !== request.workspace_id) {
      return { status: "error", error: invalid_request_error("Dirty 状态不属于当前窗口工作区") };
    }
    context.dirty_count = request.dirty_count;
    return { status: "ok", value: undefined };
  }

  dirty_count(web_contents_id: number): number {
    return this.contexts.get(web_contents_id)?.dirty_count ?? 0;
  }

  has_workspace(web_contents_id: number): boolean {
    const context = this.contexts.get(web_contents_id);
    return Boolean(context && context.renderer_workspace_id !== null);
  }

  async open_document(web_contents_id: number, value: unknown): Promise<command_result<document_snapshot>> {
    if (!is_open_document_request(value)) {
      return { status: "error", error: invalid_request_error("正文打开请求无效") };
    }
    const request = value;
    const context = this.contexts.get(web_contents_id);
    if (!context || context.workspace_id !== request.workspace_id) {
      return { status: "error", error: invalid_request_error("文档能力不属于当前窗口工作区") };
    }

    try {
      const response = await this.native_service.request("workspace.open_document", {
        window_session_id: context.window_session_id,
        workspace_id: request.workspace_id,
        target_kind: request.target_kind,
        target_id: request.target_id,
      });
      if (this.contexts.get(web_contents_id) !== context || context.workspace_id !== request.workspace_id) {
        return { status: "error", error: invalid_request_error("正文响应已经被新的工作区替换") };
      }
      const envelope = response.envelope;
      if (!envelope.ok) return { status: "error", error: native_error_to_desktop_error(envelope.error) };
      if (envelope.body === null || !is_native_document_snapshot(envelope.result)) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效正文信息") };
      }
      let content: string;
      try {
        content = new TextDecoder("utf-8", { fatal: true }).decode(response.body);
      } catch {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效 UTF-8 正文") };
      }
      const raw_hash = createHash("sha256");
      if (envelope.result.bom) raw_hash.update(Buffer.from([0xEF, 0xBB, 0xBF]));
      raw_hash.update(response.body);
      if (raw_hash.digest("hex") !== envelope.result.content_hash) {
        return { status: "error", error: unavailable_error("正文原始字节摘要与元数据不一致") };
      }
      const snapshot: document_snapshot = { ...envelope.result, content };
      if (!is_document_snapshot(snapshot)) {
        return { status: "error", error: unavailable_error("正文元数据与内容不一致") };
      }
      return { status: "ok", value: snapshot };
    } catch {
      return { status: "error", error: unavailable_error() };
    }
  }

  async save_document(web_contents_id: number, value: unknown): Promise<command_result<save_document_result>> {
    if (!is_save_document_request(value)) {
      return { status: "error", error: invalid_request_error("正文保存请求无效") };
    }
    const request = value;
    const context = this.contexts.get(web_contents_id);
    if (!context || context.workspace_id !== request.workspace_id) {
      return { status: "error", error: invalid_request_error("文档能力不属于当前窗口工作区") };
    }

    try {
      const response = await this.native_service.request("workspace.save_document", {
        window_session_id: context.window_session_id,
        workspace_id: request.workspace_id,
        document_id: request.document_id,
        expected_file_version_token: request.expected_file_version_token,
        expected_content_hash: request.expected_content_hash,
        editor_revision: request.editor_revision,
        line_ending_policy: request.line_ending_policy,
      }, { body: Buffer.from(request.content, "utf8") });
      if (this.contexts.get(web_contents_id) !== context || context.workspace_id !== request.workspace_id) {
        return { status: "error", error: invalid_request_error("保存响应已经被新的工作区替换") };
      }
      const envelope = response.envelope;
      if (!envelope.ok) return { status: "error", error: native_error_to_desktop_error(envelope.error) };
      if (envelope.body !== null || response.body.byteLength !== 0 || !is_save_document_result(envelope.result)
          || envelope.result.workspace_id !== request.workspace_id
          || envelope.result.document_id !== request.document_id
          || envelope.result.saved_revision !== request.editor_revision) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效保存状态") };
      }
      const saved_content = envelope.result.line_ending === "crlf"
        ? request.content.replaceAll("\n", "\r\n")
        : request.content;
      const raw_content = envelope.result.bom
        ? Buffer.concat([Buffer.from([0xEF, 0xBB, 0xBF]), Buffer.from(saved_content, "utf8")])
        : Buffer.from(saved_content, "utf8");
      const expected_line_ending = saved_content.includes("\r\n")
        ? "crlf"
        : saved_content.includes("\n") ? "lf" : "none";
      if (envelope.result.line_ending === "mixed"
          || envelope.result.line_ending !== expected_line_ending
          || envelope.result.byte_size !== raw_content.byteLength
          || envelope.result.content_hash !== createHash("sha256").update(raw_content).digest("hex")) {
        return { status: "error", error: unavailable_error("保存结果摘要与提交正文不一致") };
      }
      return { status: "ok", value: envelope.result };
    } catch {
      return { status: "error", error: unavailable_error("本地文件服务未能完成保存请求") };
    }
  }

  async close_document(web_contents_id: number, value: unknown): Promise<command_result<void>> {
    if (!is_close_document_request(value)) {
      return { status: "error", error: invalid_request_error("文档关闭请求无效") };
    }
    const request = value;
    const context = this.contexts.get(web_contents_id);
    if (!context || context.workspace_id !== request.workspace_id) {
      return { status: "error", error: invalid_request_error("文档能力不属于当前窗口工作区") };
    }
    try {
      const response = await this.native_service.request("workspace.close_document", {
        window_session_id: context.window_session_id,
        workspace_id: request.workspace_id,
        document_id: request.document_id,
      });
      if (this.contexts.get(web_contents_id) !== context || context.workspace_id !== request.workspace_id) {
        return { status: "error", error: invalid_request_error("文档关闭响应已经被新的工作区替换") };
      }
      const envelope = response.envelope;
      if (!envelope.ok) return { status: "error", error: native_error_to_desktop_error(envelope.error) };
      if (!is_native_close_result(envelope.result) || envelope.body !== null || response.body.byteLength !== 0) {
        return { status: "error", error: unavailable_error("本地文件服务返回了无效文档关闭状态") };
      }
      return { status: "ok", value: undefined };
    } catch {
      return { status: "error", error: unavailable_error("本地文件服务未能关闭文档") };
    }
  }
}
