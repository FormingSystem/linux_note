import type { IpcMainInvokeEvent } from "electron";

const APP_PROTOCOL = "loop-app:";
const APP_HOST = "app";

export function is_trusted_workbench_url(value: string, development_url?: string): boolean {
  try {
    const candidate = new URL(value);
    if (candidate.protocol === APP_PROTOCOL && candidate.hostname === APP_HOST) return true;
    if (!development_url) return false;
    return candidate.origin === new URL(development_url).origin;
  } catch {
    return false;
  }
}

export function assert_trusted_ipc_sender(event: IpcMainInvokeEvent, development_url?: string): void {
  const frame = event.senderFrame;
  if (!frame || frame !== event.sender.mainFrame || !is_trusted_workbench_url(frame.url, development_url)) {
    throw new Error("拒绝未授权的 IPC 调用方");
  }
}
