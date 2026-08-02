import type { IpcMainInvokeEvent } from "electron";

const APP_PROTOCOL = "loop-app:";
const APP_HOST = "app";

export function isTrustedWorkbenchUrl(value: string, developmentUrl?: string): boolean {
  try {
    const candidate = new URL(value);
    if (candidate.protocol === APP_PROTOCOL && candidate.hostname === APP_HOST) return true;
    if (!developmentUrl) return false;
    return candidate.origin === new URL(developmentUrl).origin;
  } catch {
    return false;
  }
}

export function assertTrustedIpcSender(event: IpcMainInvokeEvent, developmentUrl?: string): void {
  const frame = event.senderFrame;
  if (!frame || frame !== event.sender.mainFrame || !isTrustedWorkbenchUrl(frame.url, developmentUrl)) {
    throw new Error("拒绝未授权的 IPC 调用方");
  }
}
