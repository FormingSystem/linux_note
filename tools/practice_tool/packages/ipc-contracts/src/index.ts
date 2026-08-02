export const IPC_CHANNELS = Object.freeze({
  getRuntimeInfo: "loop:system:get-runtime-info",
});

export const NATIVE_PROTOCOL_VERSION = 1;
export const MAX_NATIVE_CONTROL_FRAME_BYTES = 1024 * 1024;

export type NativeServiceStatus = "starting" | "ready" | "missing" | "failed" | "stopped";

export interface NativeServiceInfo {
  status: NativeServiceStatus;
  protocolVersion: number | null;
  serviceVersion: string | null;
  message: string;
}

export interface RuntimeInfo {
  appName: "Loop";
  appVersion: string;
  platform: "win32" | "linux" | "darwin";
  electronVersion: string;
  nativeService: NativeServiceInfo;
}

export interface LoopDesktopApi {
  system: {
    getRuntimeInfo(): Promise<RuntimeInfo>;
  };
}

export interface NativeRequestEnvelope {
  protocol_version: number;
  request_id: string;
  method: "system.handshake";
  params: {
    client_name: "loop-desktop";
    client_version: string;
  };
}

export interface NativeResponseEnvelope {
  protocol_version: number;
  request_id: string;
  ok: boolean;
  result?: {
    service_name: "loop-native-service";
    service_version: string;
    language: "C++";
  };
  error?: {
    code: string;
    message: string;
  };
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function hasExactKeys(value: Record<string, unknown>, keys: readonly string[]): boolean {
  const actualKeys = Object.keys(value);
  return actualKeys.length === keys.length && keys.every((key) => Object.hasOwn(value, key));
}

export function isNativeResponseEnvelope(value: unknown): value is NativeResponseEnvelope {
  if (!isRecord(value)) return false;
  const envelopeKeys = value.ok === true
    ? ["protocol_version", "request_id", "ok", "result"]
    : ["protocol_version", "request_id", "ok", "error"];
  if (!hasExactKeys(value, envelopeKeys)) return false;
  if (value.protocol_version !== NATIVE_PROTOCOL_VERSION) return false;
  if (typeof value.request_id !== "string" || value.request_id.length === 0 || value.request_id.length > 128) return false;
  if (typeof value.ok !== "boolean") return false;

  if (value.ok) {
    if (!isRecord(value.result)) return false;
    return hasExactKeys(value.result, ["service_name", "service_version", "language"])
      && value.result.service_name === "loop-native-service"
      && typeof value.result.service_version === "string"
      && value.result.service_version.length > 0
      && value.result.service_version.length <= 64
      && value.result.language === "C++";
  }

  return isRecord(value.error)
    && hasExactKeys(value.error, ["code", "message"])
    && typeof value.error.code === "string"
    && value.error.code.length > 0
    && value.error.code.length <= 64
    && typeof value.error.message === "string"
    && value.error.message.length > 0
    && value.error.message.length <= 512;
}

export function isRuntimeInfo(value: unknown): value is RuntimeInfo {
  if (!isRecord(value) || !isRecord(value.nativeService)) return false;
  if (!hasExactKeys(value, ["appName", "appVersion", "platform", "electronVersion", "nativeService"])
      || !hasExactKeys(value.nativeService, ["status", "protocolVersion", "serviceVersion", "message"])) return false;
  const status = value.nativeService.status;
  return value.appName === "Loop"
    && typeof value.appVersion === "string" && value.appVersion.length > 0 && value.appVersion.length <= 64
    && ["win32", "linux", "darwin"].includes(String(value.platform))
    && typeof value.electronVersion === "string" && value.electronVersion.length > 0 && value.electronVersion.length <= 64
    && ["starting", "ready", "missing", "failed", "stopped"].includes(String(status))
    && (value.nativeService.protocolVersion === null
      || (typeof value.nativeService.protocolVersion === "number"
        && Number.isSafeInteger(value.nativeService.protocolVersion)))
    && (value.nativeService.serviceVersion === null
      || (typeof value.nativeService.serviceVersion === "string" && value.nativeService.serviceVersion.length <= 64))
    && typeof value.nativeService.message === "string"
    && value.nativeService.message.length > 0
    && value.nativeService.message.length <= 512;
}
