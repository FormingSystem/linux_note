export const IPC_CHANNELS = Object.freeze({
  get_runtime_info: "loop:system:get_runtime_info",
  open_file: "loop:workbench:open_file",
  open_folder: "loop:workbench:open_folder",
  close_workspace: "loop:workbench:close_workspace",
  list_children: "loop:explorer:list_children",
});

export const NATIVE_PROTOCOL_VERSION = 2;
export const MAX_NATIVE_CONTROL_FRAME_BYTES = 1024 * 1024;

export type native_service_status = "starting" | "ready" | "missing" | "failed" | "stopped";
export type workspace_mode = "single_file" | "folder";
export type line_ending = "lf" | "crlf" | "mixed" | "none";
export type entry_kind = "directory" | "markdown" | "file" | "symbolic_link" | "other";

export type desktop_error_code =
  | "NATIVE_UNAVAILABLE"
  | "NOT_FOUND"
  | "NOT_REGULAR_FILE"
  | "NOT_DIRECTORY"
  | "PERMISSION_DENIED"
  | "CONTENT_TOO_LARGE"
  | "INVALID_ENCODING"
  | "PATH_OUTSIDE_WORKSPACE"
  | "SYMLINK_REQUIRES_CONFIRMATION"
  | "DIRECTORY_RESOURCE_LIMIT"
  | "WORKSPACE_INVALID"
  | "INVALID_REQUEST"
  | "INTERNAL_ERROR";

export type native_error_code = desktop_error_code
  | "INVALID_JSON"
  | "INVALID_ENVELOPE"
  | "PROTOCOL_MISMATCH"
  | "INVALID_REQUEST_ID"
  | "INVALID_METHOD"
  | "INVALID_PARAMS"
  | "UNKNOWN_METHOD";

export type recovery_action = "RETRY" | "CHOOSE_ANOTHER" | "REOPEN_WORKSPACE" | "REFINE_SCOPE";

export interface desktop_error {
  code: desktop_error_code;
  user_message: string;
  retryable: boolean;
  recovery_actions: recovery_action[];
  correlation_id: string;
}

export type command_result<value_type> =
  | { status: "ok"; value: value_type }
  | { status: "cancelled" }
  | { status: "error"; error: desktop_error };

export interface native_service_info {
  status: native_service_status;
  protocol_version: number | null;
  service_version: string | null;
  message: string;
}

export interface runtime_info {
  app_name: "Loop";
  app_version: string;
  platform: "win32" | "linux" | "darwin";
  electron_version: string;
  native_service: native_service_info;
}

export interface opened_document_info {
  document_id: string;
  name: string;
  display_path: string;
  byte_size: number;
  encoding: "utf-8";
  bom: boolean;
  line_ending: line_ending;
  read_only: boolean;
  resolved_from_link: boolean;
}

export interface opened_single_file {
  workspace_id: string;
  mode: "single_file";
  display_name: string;
  document: opened_document_info;
}

export interface opened_folder {
  workspace_id: string;
  mode: "folder";
  display_name: string;
  root_directory_id: string;
  resolved_from_link: boolean;
}

export interface file_entry {
  entry_id: string;
  parent_id: string;
  name: string;
  relative_path: string;
  kind: entry_kind;
  expandable: boolean;
  accessible: boolean;
  byte_size: number | null;
}

export interface entry_page {
  workspace_id: string;
  directory_id: string;
  entries: file_entry[];
  next_cursor: string | null;
  total_entries: number;
}

export interface list_children_request {
  workspace_id: string;
  directory_id: string;
  cursor?: string;
}

export interface loop_desktop_api {
  system: {
    get_runtime_info(): Promise<runtime_info>;
  };
  workbench: {
    open_file(): Promise<command_result<opened_single_file>>;
    open_folder(): Promise<command_result<opened_folder>>;
    close_workspace(): Promise<command_result<void>>;
  };
  explorer: {
    list_children(request: list_children_request): Promise<command_result<entry_page>>;
  };
}

export type native_method =
  | "system.handshake"
  | "workspace.open_file"
  | "workspace.open_folder"
  | "workspace.close"
  | "workspace.list_children";

export interface native_request_params_by_method {
  "system.handshake": { client_name: "loop_desktop"; client_version: string };
  "workspace.open_file": { window_session_id: string; locator: string };
  "workspace.open_folder": { window_session_id: string; locator: string };
  "workspace.close": { window_session_id: string };
  "workspace.list_children": {
    window_session_id: string;
    workspace_id: string;
    directory_id: string;
    cursor?: string;
  };
}

export type native_request_envelope = {
  [method_type in native_method]: {
    protocol_version: number;
    request_id: string;
    method: method_type;
    params: native_request_params_by_method[method_type];
  }
}[native_method];

export interface native_error {
  code: native_error_code;
  user_message: string;
  retryable: boolean;
  recovery_actions: recovery_action[];
  correlation_id: string;
}

export type native_response_envelope =
  | { protocol_version: number; request_id: string; ok: true; result: unknown }
  | { protocol_version: number; request_id: string; ok: false; error: native_error };

export interface native_handshake_result {
  service_name: "loop_native_service";
  service_version: string;
  language: "C++";
}

export interface native_close_result {
  closed: true;
}

function is_record(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function has_exact_keys(value: Record<string, unknown>, required: readonly string[], optional: readonly string[] = []): boolean {
  const allowed = new Set([...required, ...optional]);
  const actual_keys = Object.keys(value);
  return required.every((key) => Object.hasOwn(value, key))
    && actual_keys.every((key) => allowed.has(key));
}

function is_bounded_string(value: unknown, maximum: number, allow_empty = false): value is string {
  return typeof value === "string" && (allow_empty || value.length > 0) && value.length <= maximum;
}

function is_opaque_id(value: unknown): value is string {
  return is_bounded_string(value, 128) && /^[a-z][a-z0-9_]*_[0-9a-f]{32}$/.test(value);
}

function is_recovery_action(value: unknown): value is recovery_action {
  return value === "RETRY" || value === "CHOOSE_ANOTHER" || value === "REOPEN_WORKSPACE" || value === "REFINE_SCOPE";
}

function is_desktop_error_code(value: unknown): value is desktop_error_code {
  return [
    "NATIVE_UNAVAILABLE", "NOT_FOUND", "NOT_REGULAR_FILE", "NOT_DIRECTORY", "PERMISSION_DENIED",
    "CONTENT_TOO_LARGE", "INVALID_ENCODING", "PATH_OUTSIDE_WORKSPACE", "SYMLINK_REQUIRES_CONFIRMATION",
    "DIRECTORY_RESOURCE_LIMIT", "WORKSPACE_INVALID", "INVALID_REQUEST", "INTERNAL_ERROR",
  ].includes(String(value));
}

function is_native_error_code(value: unknown): value is native_error_code {
  return is_desktop_error_code(value) || [
    "INVALID_JSON", "INVALID_ENVELOPE", "PROTOCOL_MISMATCH", "INVALID_REQUEST_ID",
    "INVALID_METHOD", "INVALID_PARAMS", "UNKNOWN_METHOD",
  ].includes(String(value));
}

function is_native_error(value: unknown): value is native_error {
  if (!is_record(value) || !has_exact_keys(value, ["code", "user_message", "retryable", "recovery_actions", "correlation_id"])) return false;
  return is_native_error_code(value.code)
    && is_bounded_string(value.user_message, 512)
    && typeof value.retryable === "boolean"
    && Array.isArray(value.recovery_actions)
    && value.recovery_actions.length <= 4
    && value.recovery_actions.every(is_recovery_action)
    && is_bounded_string(value.correlation_id, 128);
}

export function is_native_response_envelope(value: unknown): value is native_response_envelope {
  if (!is_record(value)) return false;
  const keys = value.ok === true
    ? ["protocol_version", "request_id", "ok", "result"]
    : ["protocol_version", "request_id", "ok", "error"];
  if (!has_exact_keys(value, keys) || Object.keys(value).length !== keys.length) return false;
  if (value.protocol_version !== NATIVE_PROTOCOL_VERSION || !is_bounded_string(value.request_id, 128) || typeof value.ok !== "boolean") return false;
  return value.ok ? is_record(value.result) : is_native_error(value.error);
}

export function is_native_handshake_result(value: unknown): value is native_handshake_result {
  return is_record(value)
    && has_exact_keys(value, ["service_name", "service_version", "language"])
    && Object.keys(value).length === 3
    && value.service_name === "loop_native_service"
    && is_bounded_string(value.service_version, 64)
    && value.language === "C++";
}

export function is_native_close_result(value: unknown): value is native_close_result {
  return is_record(value) && has_exact_keys(value, ["closed"]) && Object.keys(value).length === 1 && value.closed === true;
}

function is_opened_document_info(value: unknown): value is opened_document_info {
  if (!is_record(value) || !has_exact_keys(value, [
    "document_id", "name", "display_path", "byte_size", "encoding", "bom", "line_ending", "read_only", "resolved_from_link",
  ]) || Object.keys(value).length !== 9) return false;
  return is_opaque_id(value.document_id)
    && is_bounded_string(value.name, 1024)
    && is_bounded_string(value.display_path, 4096)
    && Number.isSafeInteger(value.byte_size) && Number(value.byte_size) >= 0 && Number(value.byte_size) <= 5 * 1024 * 1024
    && value.encoding === "utf-8"
    && typeof value.bom === "boolean"
    && ["lf", "crlf", "mixed", "none"].includes(String(value.line_ending))
    && typeof value.read_only === "boolean"
    && typeof value.resolved_from_link === "boolean";
}

export function is_opened_single_file(value: unknown): value is opened_single_file {
  return is_record(value)
    && has_exact_keys(value, ["workspace_id", "mode", "display_name", "document"])
    && Object.keys(value).length === 4
    && is_opaque_id(value.workspace_id)
    && value.mode === "single_file"
    && is_bounded_string(value.display_name, 1024)
    && is_opened_document_info(value.document);
}

export function is_opened_folder(value: unknown): value is opened_folder {
  return is_record(value)
    && has_exact_keys(value, ["workspace_id", "mode", "display_name", "root_directory_id", "resolved_from_link"])
    && Object.keys(value).length === 5
    && is_opaque_id(value.workspace_id)
    && value.mode === "folder"
    && is_bounded_string(value.display_name, 1024)
    && is_opaque_id(value.root_directory_id)
    && typeof value.resolved_from_link === "boolean";
}

function is_file_entry(value: unknown): value is file_entry {
  if (!is_record(value) || !has_exact_keys(value, [
    "entry_id", "parent_id", "name", "relative_path", "kind", "expandable", "accessible", "byte_size",
  ]) || Object.keys(value).length !== 8) return false;
  return is_opaque_id(value.entry_id)
    && is_opaque_id(value.parent_id)
    && is_bounded_string(value.name, 1024)
    && is_bounded_string(value.relative_path, 4096)
    && ["directory", "markdown", "file", "symbolic_link", "other"].includes(String(value.kind))
    && typeof value.expandable === "boolean"
    && typeof value.accessible === "boolean"
    && (value.byte_size === null || (Number.isSafeInteger(value.byte_size) && Number(value.byte_size) >= 0));
}

export function is_entry_page(value: unknown): value is entry_page {
  if (!is_record(value) || !has_exact_keys(value, ["workspace_id", "directory_id", "entries", "next_cursor", "total_entries"])
      || Object.keys(value).length !== 5) return false;
  return is_opaque_id(value.workspace_id)
    && is_opaque_id(value.directory_id)
    && Array.isArray(value.entries)
    && value.entries.length <= 256
    && value.entries.every(is_file_entry)
    && (value.next_cursor === null || is_opaque_id(value.next_cursor))
    && Number.isSafeInteger(value.total_entries)
    && Number(value.total_entries) >= value.entries.length
    && Number(value.total_entries) <= 50_000;
}

export function is_list_children_request(value: unknown): value is list_children_request {
  return is_record(value)
    && has_exact_keys(value, ["workspace_id", "directory_id"], ["cursor"])
    && is_opaque_id(value.workspace_id)
    && is_opaque_id(value.directory_id)
    && (value.cursor === undefined || is_opaque_id(value.cursor));
}

function is_desktop_error(value: unknown): value is desktop_error {
  if (!is_record(value) || !has_exact_keys(value, ["code", "user_message", "retryable", "recovery_actions", "correlation_id"])
      || Object.keys(value).length !== 5) return false;
  return is_desktop_error_code(value.code)
    && is_bounded_string(value.user_message, 512)
    && typeof value.retryable === "boolean"
    && Array.isArray(value.recovery_actions)
    && value.recovery_actions.length <= 4
    && value.recovery_actions.every(is_recovery_action)
    && is_bounded_string(value.correlation_id, 128);
}

function is_command_result<value_type>(
  value: unknown,
  is_value: (candidate: unknown) => candidate is value_type,
): value is command_result<value_type> {
  if (!is_record(value) || typeof value.status !== "string") return false;
  if (value.status === "ok") return has_exact_keys(value, ["status", "value"])
    && Object.keys(value).length === 2 && is_value(value.value);
  if (value.status === "cancelled") return has_exact_keys(value, ["status"])
    && Object.keys(value).length === 1;
  if (value.status === "error") return has_exact_keys(value, ["status", "error"])
    && Object.keys(value).length === 2 && is_desktop_error(value.error);
  return false;
}

export function is_open_file_command_result(value: unknown): value is command_result<opened_single_file> {
  return is_command_result(value, is_opened_single_file);
}

export function is_open_folder_command_result(value: unknown): value is command_result<opened_folder> {
  return is_command_result(value, is_opened_folder);
}

export function is_entry_page_command_result(value: unknown): value is command_result<entry_page> {
  return is_command_result(value, is_entry_page);
}

export function is_void_command_result(value: unknown): value is command_result<void> {
  return is_command_result(value, (candidate): candidate is void => candidate === undefined || candidate === null);
}

export function is_runtime_info(value: unknown): value is runtime_info {
  if (!is_record(value) || !is_record(value.native_service)) return false;
  if (!has_exact_keys(value, ["app_name", "app_version", "platform", "electron_version", "native_service"])
      || Object.keys(value).length !== 5
      || !has_exact_keys(value.native_service, ["status", "protocol_version", "service_version", "message"])
      || Object.keys(value.native_service).length !== 4) return false;
  const status = value.native_service.status;
  return value.app_name === "Loop"
    && is_bounded_string(value.app_version, 64)
    && ["win32", "linux", "darwin"].includes(String(value.platform))
    && is_bounded_string(value.electron_version, 64)
    && ["starting", "ready", "missing", "failed", "stopped"].includes(String(status))
    && (value.native_service.protocol_version === null
      || (typeof value.native_service.protocol_version === "number" && Number.isSafeInteger(value.native_service.protocol_version)))
    && (value.native_service.service_version === null || is_bounded_string(value.native_service.service_version, 64))
    && is_bounded_string(value.native_service.message, 512);
}

export function native_error_to_desktop_error(error: native_error): desktop_error {
  return {
    code: is_desktop_error_code(error.code) ? error.code : "INTERNAL_ERROR",
    user_message: error.user_message,
    retryable: error.retryable,
    recovery_actions: [...error.recovery_actions],
    correlation_id: error.correlation_id,
  };
}
