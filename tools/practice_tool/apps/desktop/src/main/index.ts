import { join } from "node:path";
import { randomUUID } from "node:crypto";
import {
  app,
  BrowserWindow,
  ipcMain,
  protocol,
  session,
  type WebContents,
} from "electron";
import { IPC_CHANNELS, is_runtime_info, type runtime_info } from "@loop/ipc-contracts";
import { native_service_supervisor } from "./native_service/native_service_supervisor";
import { register_app_protocol } from "./protocols/app_protocol";
import { assert_trusted_ipc_sender, is_trusted_workbench_url } from "./security/trust_policy.mts";
import { workbench_controller } from "./workbench/workbench_controller.mts";
import { electron_dialog_port } from "./workbench/electron_dialog_port";

protocol.registerSchemesAsPrivileged([
  {
    scheme: "loop-app",
    privileges: {
      standard: true,
      secure: true,
      codeCache: true,
    },
  },
]);
app.enableSandbox();

const native_service = new native_service_supervisor();
const workbench = new workbench_controller(native_service, new electron_dialog_port());
const development_url = app.isPackaged ? undefined : process.env.ELECTRON_RENDERER_URL;
const smoke_test = !app.isPackaged && process.env.LOOP_DESKTOP_SMOKE_TEST === "1";

function platform_name(): runtime_info["platform"] {
  if (process.platform === "win32" || process.platform === "linux" || process.platform === "darwin") {
    return process.platform;
  }
  throw new Error(`不支持的平台：${process.platform}`);
}

function install_navigation_guards(contents: WebContents): void {
  contents.setWindowOpenHandler(() => ({ action: "deny" }));
  contents.on("will-attach-webview", (event) => event.preventDefault());
  contents.on("will-navigate", (event, url) => {
    if (!is_trusted_workbench_url(url, development_url)) event.preventDefault();
  });
  contents.on("will-redirect", (event, url) => {
    if (!is_trusted_workbench_url(url, development_url)) event.preventDefault();
  });
}

async function create_window(): Promise<BrowserWindow> {
  const partition = `loop-workbench-${randomUUID()}`;
  const workbench_session = session.fromPartition(partition);
  workbench_session.setPermissionCheckHandler(() => false);
  workbench_session.setPermissionRequestHandler((_web_contents, _permission, callback) => callback(false));
  register_app_protocol(workbench_session, join(__dirname, "../renderer"));

  const window = new BrowserWindow({
    width: 1160,
    height: 760,
    minWidth: 640,
    minHeight: 420,
    show: false,
    backgroundColor: "#141414",
    webPreferences: {
      partition,
      preload: join(__dirname, "../preload/index.js"),
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
      webSecurity: true,
      allowRunningInsecureContent: false,
    },
  });
  const web_contents_id = window.webContents.id;
  workbench.register_window(web_contents_id);
  window.once("closed", () => workbench.unregister_window(web_contents_id));
  if (smoke_test) {
    window.webContents.on("preload-error", (_event, _preload_path, error) => {
      console.error(`Preload 加载失败：${error.message}`);
    });
  }
  install_navigation_guards(window.webContents);
  if (!smoke_test) window.once("ready-to-show", () => window.show());

  if (development_url) {
    await window.loadURL(development_url);
  } else {
    await window.loadURL("loop-app://app/");
  }
  return window;
}

function register_ipc(): void {
  ipcMain.handle(IPC_CHANNELS.get_runtime_info, (event): runtime_info => {
    assert_trusted_ipc_sender(event, development_url);
    return {
      app_name: "Loop",
      app_version: app.getVersion(),
      platform: platform_name(),
      electron_version: process.versions.electron,
      native_service: native_service.snapshot(),
    };
  });
  ipcMain.handle(IPC_CHANNELS.open_file, async (event) => {
    assert_trusted_ipc_sender(event, development_url);
    const owner = BrowserWindow.fromWebContents(event.sender);
    if (!owner) throw new Error("拒绝无窗口的打开文件请求");
    return workbench.open_file(event.sender.id, owner);
  });
  ipcMain.handle(IPC_CHANNELS.open_folder, async (event) => {
    assert_trusted_ipc_sender(event, development_url);
    const owner = BrowserWindow.fromWebContents(event.sender);
    if (!owner) throw new Error("拒绝无窗口的打开文件夹请求");
    return workbench.open_folder(event.sender.id, owner);
  });
  ipcMain.handle(IPC_CHANNELS.close_workspace, async (event) => {
    assert_trusted_ipc_sender(event, development_url);
    return workbench.close_workspace(event.sender.id);
  });
  ipcMain.handle(IPC_CHANNELS.list_children, async (event, request: unknown) => {
    assert_trusted_ipc_sender(event, development_url);
    return workbench.list_children(event.sender.id, request);
  });
}

app.whenReady().then(async () => {
  register_ipc();
  await native_service.start();
  const window = await create_window();
  if (smoke_test) {
    const runtime: unknown = await window.webContents.executeJavaScript(
      `typeof window.loop === 'object'
        && typeof window.loop.system?.get_runtime_info === 'function'
        && typeof window.loop.workbench?.open_file === 'function'
        && typeof window.loop.workbench?.open_folder === 'function'
        && typeof window.loop.workbench?.close_workspace === 'function'
        && typeof window.loop.explorer?.list_children === 'function'
        ? window.loop.system.get_runtime_info()
        : ({ smoke_error: 'PRELOAD_BRIDGE_MISSING' })`,
      true,
    );
    if (!is_runtime_info(runtime) || runtime.native_service.status !== "ready") {
      const smoke_error = typeof runtime === "object" && runtime !== null && "smoke_error" in runtime
        ? String(runtime.smoke_error)
        : "INVALID_RUNTIME_STATE";
      throw new Error(`桌面烟雾测试未通过：${smoke_error}`);
    }
    app.exit(0);
    return;
  }

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) void create_window();
  });
}).catch((error: unknown) => {
  if (smoke_test) {
    console.error(error instanceof Error ? error.message : "桌面烟雾测试失败");
  }
  app.exit(1);
});

app.on("before-quit", () => native_service.stop());
app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});
