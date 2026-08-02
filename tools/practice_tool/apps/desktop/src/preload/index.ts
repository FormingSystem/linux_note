import { contextBridge, ipcRenderer } from "electron";
import {
  IPC_CHANNELS,
  is_entry_page_command_result,
  is_list_children_request,
  is_open_file_command_result,
  is_open_folder_command_result,
  is_runtime_info,
  is_void_command_result,
  type list_children_request,
  type loop_desktop_api,
} from "@loop/ipc-contracts";

const api: loop_desktop_api = Object.freeze({
  system: Object.freeze({
    async get_runtime_info() {
      const value: unknown = await ipcRenderer.invoke(IPC_CHANNELS.get_runtime_info);
      if (!is_runtime_info(value)) throw new Error("桌面运行时返回了无效状态");
      return value;
    },
  }),
  workbench: Object.freeze({
    async open_file() {
      const value: unknown = await ipcRenderer.invoke(IPC_CHANNELS.open_file);
      if (!is_open_file_command_result(value)) throw new Error("打开文件返回了无效状态");
      return value;
    },
    async open_folder() {
      const value: unknown = await ipcRenderer.invoke(IPC_CHANNELS.open_folder);
      if (!is_open_folder_command_result(value)) throw new Error("打开文件夹返回了无效状态");
      return value;
    },
    async close_workspace() {
      const value: unknown = await ipcRenderer.invoke(IPC_CHANNELS.close_workspace);
      if (!is_void_command_result(value)) throw new Error("关闭工作区返回了无效状态");
      return value;
    },
  }),
  explorer: Object.freeze({
    async list_children(request: list_children_request) {
      if (!is_list_children_request(request)) throw new Error("目录枚举请求无效");
      const value: unknown = await ipcRenderer.invoke(IPC_CHANNELS.list_children, request);
      if (!is_entry_page_command_result(value)) throw new Error("目录枚举返回了无效状态");
      return value;
    },
  }),
});

contextBridge.exposeInMainWorld("loop", api);
