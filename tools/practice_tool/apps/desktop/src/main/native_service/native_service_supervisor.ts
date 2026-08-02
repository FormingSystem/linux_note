import { access } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { randomUUID } from "node:crypto";
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { app } from "electron";
import {
  NATIVE_PROTOCOL_VERSION,
  is_native_handshake_result,
  is_native_response_envelope,
  type native_method,
  type native_request_envelope,
  type native_request_params_by_method,
  type native_response_envelope,
  type native_service_info,
} from "@loop/ipc-contracts";
import { encode_native_frame, native_frame_decoder } from "./framing.mts";

const HANDSHAKE_TIMEOUT_MS = 3_000;
const DEFAULT_REQUEST_TIMEOUT_MS = 15_000;

type pending_request = {
  resolve: (value: native_response_envelope) => void;
  reject: (error: Error) => void;
  timer: NodeJS.Timeout;
};

export class native_service_supervisor {
  private child: ChildProcessWithoutNullStreams | null = null;
  private readonly decoder = new native_frame_decoder();
  private readonly pending = new Map<string, pending_request>();
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
    this.child = child;
    child.stderr.resume();
    child.stdout.on("data", (chunk: Buffer) => this.on_data(chunk));
    child.once("error", () => this.fail("C++ Native Service 启动失败"));
    child.once("exit", () => {
      if (this.state.status !== "stopped") this.fail("C++ Native Service 已退出");
      this.child = null;
    });

    try {
      const response = await this.handshake();
      if (!response.ok || !is_native_handshake_result(response.result)) {
        this.fail("C++ Native Service 握手被拒绝");
        return;
      }
      this.state = {
        status: "ready",
        protocol_version: response.protocol_version,
        service_version: response.result.service_version,
        message: `C++ Native Service ${response.result.service_version} 已连接`,
      };
    } catch {
      this.fail("C++ Native Service 握手失败");
    }
  }

  stop(): void {
    this.reject_pending(new Error("Native Service 已停止"));
    this.state = {
      status: "stopped",
      protocol_version: null,
      service_version: null,
      message: "Native Service 已停止",
    };
    this.child?.kill();
    this.child = null;
  }

  async request<method_type extends native_method>(
    method: method_type,
    params: native_request_params_by_method[method_type],
    timeout_ms = DEFAULT_REQUEST_TIMEOUT_MS,
  ): Promise<native_response_envelope> {
    const request = {
      protocol_version: NATIVE_PROTOCOL_VERSION,
      request_id: randomUUID(),
      method,
      params,
    } as native_request_envelope;
    return this.send(request, timeout_ms);
  }

  private resolve_executable(): string {
    const executable_name = process.platform === "win32"
      ? "loop_native_service.exe"
      : "loop_native_service";
    if (app.isPackaged) return join(process.resourcesPath, "native", executable_name);

    const preset_directory = process.platform === "win32" ? "windows-mingw" : "linux-gcc";
    return resolve(app.getAppPath(), "..", "..", "native", "build", preset_directory, executable_name);
  }

  private handshake(): Promise<native_response_envelope> {
    return this.request("system.handshake", {
      client_name: "loop_desktop",
      client_version: app.getVersion(),
    }, HANDSHAKE_TIMEOUT_MS);
  }

  private send(request: native_request_envelope, timeout_ms: number): Promise<native_response_envelope> {
    if (!this.child?.stdin.writable) return Promise.reject(new Error("Native Service 不可写"));

    return new Promise((resolve_request, reject_request) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.request_id);
        reject_request(new Error("Native Service 请求超时"));
      }, timeout_ms);
      timer.unref();
      this.pending.set(request.request_id, { resolve: resolve_request, reject: reject_request, timer });
      this.child?.stdin.write(encode_native_frame(request));
    });
  }

  private on_data(chunk: Buffer): void {
    try {
      for (const value of this.decoder.push(chunk)) {
        if (!is_native_response_envelope(value)) throw new Error("Native Service 响应无效");
        const request = this.pending.get(value.request_id);
        if (!request) continue;
        clearTimeout(request.timer);
        this.pending.delete(value.request_id);
        request.resolve(value);
      }
    } catch {
      this.fail("C++ Native Service 返回了无效协议帧");
    }
  }

  private fail(message: string): void {
    this.reject_pending(new Error(message));
    this.state = {
      status: "failed",
      protocol_version: null,
      service_version: null,
      message,
    };
    this.child?.kill();
  }

  private reject_pending(error: Error): void {
    for (const request of this.pending.values()) {
      clearTimeout(request.timer);
      request.reject(error);
    }
    this.pending.clear();
  }
}
