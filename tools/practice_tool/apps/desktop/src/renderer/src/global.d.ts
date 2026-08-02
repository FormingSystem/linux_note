import type { loop_desktop_api } from "@loop/ipc-contracts";

declare global {
  interface Window {
    loop: loop_desktop_api;
  }
}

export {};
