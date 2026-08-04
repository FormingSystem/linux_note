import assert from "node:assert/strict";
import test from "node:test";
import {
  IPC_CHANNELS,
  MAX_NATIVE_BODY_FRAME_BYTES,
  MAX_NATIVE_CONTROL_FRAME_BYTES,
  NATIVE_PROTOCOL_VERSION,
  is_entry_page_command_result,
  is_close_document_request,
  is_list_children_request,
  is_document_snapshot_command_result,
  is_native_handshake_result,
  is_native_response_envelope,
  is_open_file_command_result,
  is_open_folder_command_result,
  is_open_document_request,
  is_report_dirty_state_request,
  is_save_document_command_result,
  is_save_document_request,
  is_runtime_info,
  is_void_command_result,
} from "../dist/index.js";

const ids = Object.freeze({
  workspace: "workspace_0123456789abcdef0123456789abcdef",
  document: "document_0123456789abcdef0123456789abcdef",
  directory: "directory_0123456789abcdef0123456789abcdef",
  entry: "entry_0123456789abcdef0123456789abcdef",
  cursor: "cursor_0123456789abcdef0123456789abcdef",
});

const handshake = Object.freeze({
  service_name: "loop_native_service",
  service_version: "0.4.0",
  language: "C++",
  max_control_frame_bytes: MAX_NATIVE_CONTROL_FRAME_BYTES,
  max_body_frame_bytes: MAX_NATIVE_BODY_FRAME_BYTES,
});

const native_success = Object.freeze({
  protocol_version: NATIVE_PROTOCOL_VERSION,
  request_id: "contract-test",
  ok: true,
  body: null,
  result: handshake,
});

const valid_error = Object.freeze({
  code: "NOT_FOUND",
  user_message: "目标不存在",
  retryable: true,
  recovery_actions: ["RETRY", "CHOOSE_ANOTHER"],
  correlation_id: "contract-test",
});

test("IPC 只公开固定用例通道", () => {
  assert.deepEqual(Object.keys(IPC_CHANNELS).sort(), [
    "close_document",
    "close_workspace",
    "get_runtime_info",
    "list_children",
    "open_document",
    "open_file",
    "open_folder",
    "report_dirty_state",
    "save_document",
  ]);
  assert.equal(Object.values(IPC_CHANNELS).some((channel) => /path|read-file|write-file|invoke/i.test(channel)), false);
});

test("接受字段完整且版本匹配的 Native 响应", () => {
  assert.equal(is_native_response_envelope(native_success), true);
  assert.equal(is_native_handshake_result(handshake), true);
});

test("拒绝 Native envelope 和握手结果中的未知字段", () => {
  assert.equal(is_native_response_envelope({ ...native_success, path: "C:/not-allowed" }), false);
  assert.equal(is_native_handshake_result({ ...handshake, pointer: "0x1234" }), false);
});

test("拒绝版本不匹配和不完整 Native 错误", () => {
  assert.equal(is_native_response_envelope({ ...native_success, protocol_version: 1 }), false);
  assert.equal(is_native_response_envelope({
    protocol_version: NATIVE_PROTOCOL_VERSION,
    request_id: "contract-test",
    ok: false,
    body: null,
    error: { code: "NOT_FOUND", user_message: "" },
  }), false);
});

test("正文请求、Dirty 报告和正文快照使用严格 Schema", () => {
  assert.equal(is_open_document_request({
    workspace_id: ids.workspace,
    target_kind: "entry",
    target_id: ids.entry,
  }), true);
  assert.equal(is_open_document_request({
    workspace_id: ids.workspace,
    target_kind: "path",
    target_id: ids.entry,
  }), false);
  assert.equal(is_report_dirty_state_request({ workspace_id: ids.workspace, dirty_count: 2 }), true);
  assert.equal(is_report_dirty_state_request({ workspace_id: ids.workspace, dirty_count: -1 }), false);

  const content = "hello";
  const snapshot = {
    workspace_id: ids.workspace,
    document_id: ids.document,
    name: "README.md",
    display_path: "README.md",
    content,
    content_hash: "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
    file_version_token: "version_0123456789abcdef0123456789abcdef",
    byte_size: 5,
    modified_time_ms: 1,
    encoding: "utf-8",
    bom: false,
    line_ending: "none",
    read_only: false,
    resolved_from_link: false,
  };
  assert.equal(is_document_snapshot_command_result({ status: "ok", value: snapshot }), true);
  assert.equal(is_document_snapshot_command_result({ status: "ok", value: { ...snapshot, absolute_path: "C:/README.md" } }), false);
  assert.equal(is_document_snapshot_command_result({ status: "ok", value: { ...snapshot, byte_size: 6 } }), false);
});

test("保存请求和结果使用严格 Schema 且不接受路径", () => {
  assert.equal(is_close_document_request({ workspace_id: ids.workspace, document_id: ids.document }), true);
  assert.equal(is_close_document_request({ workspace_id: ids.workspace, document_id: ids.document, path: "README.md" }), false);
  const request = {
    workspace_id: ids.workspace,
    document_id: ids.document,
    expected_file_version_token: "version_0123456789abcdef0123456789abcdef",
    expected_content_hash: "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
    editor_revision: 3,
    line_ending_policy: "preserve",
    content: "hello",
  };
  assert.equal(is_save_document_request(request), true);
  assert.equal(is_save_document_request({ ...request, path: "C:/README.md" }), false);
  assert.equal(is_save_document_request({ ...request, expected_content_hash: "G".repeat(64) }), false);
  assert.equal(is_save_document_request({ ...request, content: "a\r\nb" }), false);
  assert.equal(is_save_document_request({ ...request, content: "x".repeat(MAX_NATIVE_BODY_FRAME_BYTES + 1) }), false);

  const result = {
    workspace_id: ids.workspace,
    document_id: ids.document,
    content_hash: request.expected_content_hash,
    file_version_token: "version_11111111111111111111111111111111",
    saved_revision: 3,
    byte_size: 5,
    modified_time_ms: 2,
    encoding: "utf-8",
    bom: false,
    line_ending: "none",
    read_only: false,
    resolved_from_link: false,
  };
  assert.equal(is_save_document_command_result({ status: "ok", value: result }), true);
  assert.equal(is_save_document_command_result({ status: "ok", value: { ...result, absolute_path: "C:/README.md" } }), false);
});

test("严格验证打开文件、文件夹和关闭结果", () => {
  const file = {
    workspace_id: ids.workspace,
    mode: "single_file",
    display_name: "README.md",
    document: {
      document_id: ids.document,
      name: "README.md",
      display_path: "README.md",
      byte_size: 1024,
      encoding: "utf-8",
      bom: false,
      line_ending: "lf",
      read_only: false,
      resolved_from_link: false,
    },
  };
  const folder = {
    workspace_id: ids.workspace,
    mode: "folder",
    display_name: "notes",
    root_directory_id: ids.directory,
    resolved_from_link: false,
  };
  assert.equal(is_open_file_command_result({ status: "ok", value: file }), true);
  assert.equal(is_open_file_command_result({ status: "ok", value: { ...file, absolute_path: "C:/notes/README.md" } }), false);
  assert.equal(is_open_file_command_result({ status: "ok", value: { ...file, display_name: "x".repeat(1025) } }), false);
  assert.equal(is_open_folder_command_result({ status: "ok", value: folder }), true);
  assert.equal(is_open_folder_command_result({ status: "error", error: valid_error }), true);
  assert.equal(is_open_folder_command_result({ status: "error", error: { ...valid_error, stack: "secret" } }), false);
  assert.equal(is_open_folder_command_result({ status: "error", error: { ...valid_error, code: "OS_ERROR" } }), false);
  assert.equal(is_void_command_result({ status: "ok", value: undefined }), true);
});

test("目录请求不接受路径且分页结果最多 256 项", () => {
  const request = { workspace_id: ids.workspace, directory_id: ids.directory };
  assert.equal(is_list_children_request(request), true);
  assert.equal(is_list_children_request({ ...request, path: "../escape" }), false);

  const entry = {
    entry_id: ids.entry,
    parent_id: ids.directory,
    name: "P01.md",
    relative_path: "P01.md",
    kind: "markdown",
    expandable: false,
    accessible: true,
    byte_size: 100,
  };
  const page = {
    workspace_id: ids.workspace,
    directory_id: ids.directory,
    entries: [entry],
    next_cursor: ids.cursor,
    total_entries: 300,
  };
  assert.equal(is_entry_page_command_result({ status: "ok", value: page }), true);
  assert.equal(is_entry_page_command_result({ status: "ok", value: { ...page, absolute_root: "C:/notes" } }), false);
  assert.equal(is_entry_page_command_result({ status: "ok", value: { ...page, entries: Array(257).fill(entry) } }), false);
});

test("Preload 拒绝夹带未知字段的运行状态", () => {
  const runtime = {
    app_name: "Loop",
    app_version: "0.1.0",
    platform: "win32",
    electron_version: "43.2.0",
    native_service: {
      status: "ready",
      protocol_version: NATIVE_PROTOCOL_VERSION,
      service_version: "0.4.0",
      message: "ready",
    },
  };
  assert.equal(is_runtime_info(runtime), true);
  assert.equal(is_runtime_info({ ...runtime, absolute_path: "C:/not-allowed" }), false);
  assert.equal(is_runtime_info({
    ...runtime,
    native_service: { ...runtime.native_service, message: "x".repeat(513) },
  }), false);
});
