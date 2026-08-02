import assert from "node:assert/strict";
import test from "node:test";
import {
  IPC_CHANNELS,
  NATIVE_PROTOCOL_VERSION,
  is_entry_page_command_result,
  is_list_children_request,
  is_native_handshake_result,
  is_native_response_envelope,
  is_open_file_command_result,
  is_open_folder_command_result,
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
  service_version: "0.2.0",
  language: "C++",
});

const native_success = Object.freeze({
  protocol_version: NATIVE_PROTOCOL_VERSION,
  request_id: "contract-test",
  ok: true,
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
    "close_workspace",
    "get_runtime_info",
    "list_children",
    "open_file",
    "open_folder",
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
    error: { code: "NOT_FOUND", user_message: "" },
  }), false);
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
      service_version: "0.2.0",
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
