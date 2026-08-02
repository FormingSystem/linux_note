import type { LoopDesktopApi } from "@loop/ipc-contracts";

declare global {
  interface Window {
    loop: LoopDesktopApi;
  }
}

export {};
