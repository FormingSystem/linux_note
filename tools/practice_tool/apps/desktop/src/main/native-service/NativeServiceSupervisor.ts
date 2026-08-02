import { access } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { randomUUID } from "node:crypto";
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { app } from "electron";
import {
  NATIVE_PROTOCOL_VERSION,
  isNativeResponseEnvelope,
  type NativeRequestEnvelope,
  type NativeResponseEnvelope,
  type NativeServiceInfo,
} from "@loop/ipc-contracts";
import { encodeNativeFrame, NativeFrameDecoder } from "./framing";

const HANDSHAKE_TIMEOUT_MS = 3_000;

type PendingRequest = {
  resolve: (value: NativeResponseEnvelope) => void;
  reject: (error: Error) => void;
  timer: NodeJS.Timeout;
};

export class NativeServiceSupervisor {
  private child: ChildProcessWithoutNullStreams | null = null;
  private readonly decoder = new NativeFrameDecoder();
  private readonly pending = new Map<string, PendingRequest>();
  private state: NativeServiceInfo = {
    status: "stopped",
    protocolVersion: null,
    serviceVersion: null,
    message: "Native Service 尚未启动",
  };

  snapshot(): NativeServiceInfo {
    return { ...this.state };
  }

  async start(): Promise<void> {
    if (this.child) return;
    this.state = {
      status: "starting",
      protocolVersion: null,
      serviceVersion: null,
      message: "正在启动 C++ Native Service",
    };

    const executable = this.resolveExecutable();
    try {
      await access(executable);
    } catch {
      this.state = {
        status: "missing",
        protocolVersion: null,
        serviceVersion: null,
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
    child.stdout.on("data", (chunk: Buffer) => this.onData(chunk));
    child.once("error", () => this.fail("C++ Native Service 启动失败"));
    child.once("exit", () => {
      if (this.state.status !== "stopped") this.fail("C++ Native Service 已退出");
      this.child = null;
    });

    try {
      const response = await this.handshake();
      if (!response.ok || !response.result) {
        this.fail("C++ Native Service 握手被拒绝");
        return;
      }
      this.state = {
        status: "ready",
        protocolVersion: response.protocol_version,
        serviceVersion: response.result.service_version,
        message: `C++ Native Service ${response.result.service_version} 已连接`,
      };
    } catch {
      this.fail("C++ Native Service 握手失败");
    }
  }

  stop(): void {
    this.rejectPending(new Error("Native Service 已停止"));
    this.state = {
      status: "stopped",
      protocolVersion: null,
      serviceVersion: null,
      message: "Native Service 已停止",
    };
    this.child?.kill();
    this.child = null;
  }

  private resolveExecutable(): string {
    const executableName = process.platform === "win32"
      ? "loop_native_service.exe"
      : "loop_native_service";
    if (app.isPackaged) return join(process.resourcesPath, "native", executableName);

    const presetDirectory = process.platform === "win32" ? "windows-mingw" : "linux-gcc";
    return resolve(app.getAppPath(), "..", "..", "native", "build", presetDirectory, executableName);
  }

  private handshake(): Promise<NativeResponseEnvelope> {
    const requestId = randomUUID();
    const request: NativeRequestEnvelope = {
      protocol_version: NATIVE_PROTOCOL_VERSION,
      request_id: requestId,
      method: "system.handshake",
      params: {
        client_name: "loop-desktop",
        client_version: app.getVersion(),
      },
    };
    return this.send(request);
  }

  private send(request: NativeRequestEnvelope): Promise<NativeResponseEnvelope> {
    if (!this.child?.stdin.writable) return Promise.reject(new Error("Native Service 不可写"));

    return new Promise((resolveRequest, rejectRequest) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.request_id);
        rejectRequest(new Error("Native Service 请求超时"));
      }, HANDSHAKE_TIMEOUT_MS);
      timer.unref();
      this.pending.set(request.request_id, { resolve: resolveRequest, reject: rejectRequest, timer });
      this.child?.stdin.write(encodeNativeFrame(request));
    });
  }

  private onData(chunk: Buffer): void {
    try {
      for (const value of this.decoder.push(chunk)) {
        if (!isNativeResponseEnvelope(value)) throw new Error("Native Service 响应无效");
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
    this.rejectPending(new Error(message));
    this.state = {
      status: "failed",
      protocolVersion: null,
      serviceVersion: null,
      message,
    };
    this.child?.kill();
  }

  private rejectPending(error: Error): void {
    for (const request of this.pending.values()) {
      clearTimeout(request.timer);
      request.reject(error);
    }
    this.pending.clear();
  }
}
