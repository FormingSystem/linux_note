import { contextBridge, ipcRenderer } from "electron";
import {
  IPC_CHANNELS,
  isRuntimeInfo,
  type LoopDesktopApi,
} from "@loop/ipc-contracts";

const api: LoopDesktopApi = Object.freeze({
  system: Object.freeze({
    async getRuntimeInfo() {
      const value: unknown = await ipcRenderer.invoke(IPC_CHANNELS.getRuntimeInfo);
      if (!isRuntimeInfo(value)) throw new Error("桌面运行时返回了无效状态");
      return value;
    },
  }),
});

contextBridge.exposeInMainWorld("loop", api);
