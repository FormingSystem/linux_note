import { access } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { createHash, randomUUID } from "node:crypto";
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { app } from "electron";
import {
  NATIVE_PROTOCOL_VERSION,
  is_native_handshake_result,
  type native_method,
  type native_request_envelope,
  type native_request_params_by_method,
  type native_response_envelope,
  type native_service_info,
} from "@loop/ipc-contracts";
import { encode_native_frame, native_frame_decoder } from "./framing.mts";
import { validate_native_response_frame } from "./response_validation.mts";

const HANDSHAKE_TIMEOUT_MS = 3_000;
const DEFAULT_REQUEST_TIMEOUT_MS = 15_000;
const MAXIMUM_PENDING_REQUESTS = 64;
const MAXIMUM_QUEUED_WRITE_BYTES = 8 * 1024 * 1024;
const LATE_RESPONSE_RETENTION_MS = 30_000;

export interface native_service_response {
  envelope: native_response_envelope;
  body: Buffer;
}

export interface native_request_options {
  body?: Buffer;
  timeout_ms?: number;
}

type pending_request = {
  method: native_method;
  resolve: (value: native_service_response) => void;
  reject: (error: Error) => void;
  timer: NodeJS.Timeout;
};

type expired_request = {
  method: native_method;
  timer: NodeJS.Timeout;
};

export class native_service_supervisor {
  private child: ChildProcessWithoutNullStreams | null = null;
  private service_generation = 0;
  private readonly pending = new Map<string, pending_request>();
  private readonly expired = new Map<string, expired_request>();
  private write_queue: Promise<void> = Promise.resolve();
  private queued_write_bytes = 0;
  private state: native_service_info = {
    status: "stopped",
    protocol_version: null,
    service_version: null,
    message: "Native Service 尚未启动",
  };

  snapshot(): native_service_info {
    return { ...this.state };
  }

  async start(): Promise<void> {
    if (this.child) return;
    const service_generation = ++this.service_generation;
    this.state = {
      status: "starting",
      protocol_version: null,
      service_version: null,
      message: "正在启动 C++ Native Service",
    };

    const executable = this.resolve_executable();
    try {
      await access(executable);
    } catch {
      this.state = {
        status: "missing",
        protocol_version: null,
        service_version: null,
        message: "尚未构建 C++ Native Service",
      };
      return;
    }

    const child = spawn(executable, ["--stdio"], {
      cwd: dirname(executable),
      shell: false,
      windowsHide: true,
      stdio: ["pipe", "pipe", "pipe"],
    });
    const decoder = new native_frame_decoder();
    this.child = child;
    child.stderr.resume();
    child.stdout.on("data", (chunk: Buffer) => this.on_data(decoder, chunk, service_generation));
    child.stdout.once("end", () => {
      if (service_generation !== this.service_generation || this.child !== child) return;
      try {
        decoder.finish();
      } catch {
        this.fail("C++ Native Service 输出了截断协议帧", service_generation);
      }
    });
    child.once("error", () => this.fail("C++ Native Service 启动失败", service_generation));
    child.once("exit", () => {
      if (service_generation !== this.service_generation || this.child !== child) return;
      if (this.state.status !== "stopped") this.fail("C++ Native Service 已退出", service_generation);
    });

    try {
      const response = await this.handshake();
      if (service_generation !== this.service_generation || this.child !== child) return;
      if (!response.envelope.ok || !is_native_handshake_result(response.envelope.result)) {
        this.fail("C++ Native Service 握手被拒绝", service_generation);
        return;
      }
      this.state = {
        status: "ready",
        protocol_version: response.envelope.protocol_version,
        service_version: response.envelope.result.service_version,
        message: `C++ Native Service ${response.envelope.result.service_version} 已连接`,
      };
    } catch {
      this.fail("C++ Native Service 握手失败", service_generation);
    }
  }

  stop(): void {
    ++this.service_generation;
    this.reject_pending(new Error("Native Service 已停止"));
    this.state = {
      status: "stopped",
      protocol_version: null,
      service_version: null,
      message: "Native Service 已停止",
    };
    const child = this.child;
    this.child = null;
    child?.kill();
  }

  async request<method_type extends native_method>(
    method: method_type,
    params: native_request_params_by_method[method_type],
    options: native_request_options = {},
  ): Promise<native_service_response> {
    const body = options.body;
    const body_descriptor = body === undefined ? null : {
      kind: "markdown_source_utf8" as const,
      byte_length: body.byteLength,
      sha256: createHash("sha256").update(body).digest("hex"),
    };
    if ((method === "workspace.save_document") !== (body !== undefined)) {
      throw new Error("Native Service 请求正文方向无效");
    }
    const request = {
      protocol_version: NATIVE_PROTOCOL_VERSION,
      request_id: randomUUID(),
      method,
      params,
      body: body_descriptor,
    } as native_request_envelope;
    return this.send(request, body, options.timeout_ms ?? DEFAULT_REQUEST_TIMEOUT_MS);
  }

  private resolve_executable(): string {
    const executable_name = process.platform === "win32"
      ? "loop_native_service.exe"
      : "loop_native_service";
    if (app.isPackaged) return join(process.resourcesPath, "native", executable_name);

    const preset_directory = process.platform === "win32" ? "windows-mingw" : "linux-gcc";
    return resolve(app.getAppPath(), "..", "..", "native", "build", preset_directory, executable_name);
  }

  private handshake(): Promise<native_service_response> {
    return this.request("system.handshake", {
      client_name: "loop_desktop",
      client_version: app.getVersion(),
    }, { timeout_ms: HANDSHAKE_TIMEOUT_MS });
  }

  private send(request: native_request_envelope, body: Buffer | undefined, timeout_ms: number): Promise<native_service_response> {
    const child = this.child;
    const service_generation = this.service_generation;
    if (!child?.stdin.writable) return Promise.reject(new Error("Native Service 不可写"));
    if (this.pending.size >= MAXIMUM_PENDING_REQUESTS) {
      return Promise.reject(new Error("Native Service 待处理请求已达到上限"));
    }
    const encoded = encode_native_frame(request, body, body !== undefined);
    if (this.queued_write_bytes + encoded.byteLength > MAXIMUM_QUEUED_WRITE_BYTES) {
      return Promise.reject(new Error("Native Service 写入队列已达到上限"));
    }

    return new Promise((resolve_request, reject_request) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.request_id);
        this.remember_expired_request(request.request_id, request.method);
        reject_request(new Error("Native Service 请求超时"));
      }, timeout_ms);
      timer.unref();
      this.pending.set(request.request_id, {
        method: request.method,
        resolve: resolve_request,
        reject: reject_request,
        timer,
      });
      this.queued_write_bytes += encoded.byteLength;
      this.write_queue = this.write_queue
        .then(() => this.write_frame(child, service_generation, encoded))
        .catch((error: unknown) => {
          const pending_request = this.pending.get(request.request_id);
          if (pending_request) {
            clearTimeout(pending_request.timer);
            this.pending.delete(request.request_id);
            pending_request.reject(error instanceof Error ? error : new Error("Native Service 写入失败"));
          }
          this.fail("C++ Native Service 写入失败", service_generation);
        })
        .finally(() => {
          this.queued_write_bytes -= encoded.byteLength;
        });
    });
  }

  private write_frame(
    child: ChildProcessWithoutNullStreams,
    service_generation: number,
    frame: Buffer,
  ): Promise<void> {
    if (service_generation !== this.service_generation || this.child !== child || !child.stdin.writable) {
      return Promise.reject(new Error("Native Service 不可写"));
    }
    return new Promise((resolve_write, reject_write) => {
      child.stdin.write(frame, (error) => {
        if (error) reject_write(error);
        else resolve_write();
      });
    });
  }

  private on_data(decoder: native_frame_decoder, chunk: Buffer, service_generation: number): void {
    if (service_generation !== this.service_generation) return;
    try {
      for (const frame of decoder.push(chunk)) {
        if (typeof frame.control !== "object" || frame.control === null || !("request_id" in frame.control)) {
          throw new Error("Native Service 响应无效");
        }
        const request_id = String(frame.control.request_id);
        const request = this.pending.get(request_id);
        const expired = this.expired.get(request_id);
        const validated = validate_native_response_frame(frame, request?.method ?? expired?.method ?? null);
        const value = validated.envelope;
        if (expired) {
          clearTimeout(expired.timer);
          this.expired.delete(request_id);
        }
        if (!request) continue;
        clearTimeout(request.timer);
        this.pending.delete(value.request_id);
        request.resolve(validated);
      }
    } catch {
      this.fail("C++ Native Service 返回了无效协议帧", service_generation);
    }
  }

  private fail(message: string, service_generation: number): void {
    if (service_generation !== this.service_generation) return;
    this.reject_pending(new Error(message));
    this.state = {
      status: "failed",
      protocol_version: null,
      service_version: null,
      message,
    };
    const child = this.child;
    this.child = null;
    ++this.service_generation;
    child?.kill();
  }

  private reject_pending(error: Error): void {
    for (const request of this.pending.values()) {
      clearTimeout(request.timer);
      request.reject(error);
    }
    this.pending.clear();
    for (const request of this.expired.values()) clearTimeout(request.timer);
    this.expired.clear();
  }

  private remember_expired_request(request_id: string, method: native_method): void {
    while (this.expired.size >= MAXIMUM_PENDING_REQUESTS) {
      const oldest = this.expired.entries().next().value as [string, expired_request] | undefined;
      if (!oldest) break;
      clearTimeout(oldest[1].timer);
      this.expired.delete(oldest[0]);
    }
    const timer = setTimeout(() => this.expired.delete(request_id), LATE_RESPONSE_RETENTION_MS);
    timer.unref();
    this.expired.set(request_id, { method, timer });
  }
}
