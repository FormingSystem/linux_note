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
import { IPC_CHANNELS, isRuntimeInfo, type RuntimeInfo } from "@loop/ipc-contracts";
import { NativeServiceSupervisor } from "./native-service/NativeServiceSupervisor";
import { registerAppProtocol } from "./protocols/appProtocol";
import { assertTrustedIpcSender, isTrustedWorkbenchUrl } from "./security/trustPolicy";

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

const nativeService = new NativeServiceSupervisor();
const developmentUrl = app.isPackaged ? undefined : process.env.ELECTRON_RENDERER_URL;
const smokeTest = !app.isPackaged && process.env.LOOP_DESKTOP_SMOKE_TEST === "1";

function platformName(): RuntimeInfo["platform"] {
  if (process.platform === "win32" || process.platform === "linux" || process.platform === "darwin") {
    return process.platform;
  }
  throw new Error(`不支持的平台：${process.platform}`);
}

function installNavigationGuards(contents: WebContents): void {
  contents.setWindowOpenHandler(() => ({ action: "deny" }));
  contents.on("will-attach-webview", (event) => event.preventDefault());
  contents.on("will-navigate", (event, url) => {
    if (!isTrustedWorkbenchUrl(url, developmentUrl)) event.preventDefault();
  });
  contents.on("will-redirect", (event, url) => {
    if (!isTrustedWorkbenchUrl(url, developmentUrl)) event.preventDefault();
  });
}

async function createWindow(): Promise<BrowserWindow> {
  const partition = `loop-workbench-${randomUUID()}`;
  const workbenchSession = session.fromPartition(partition);
  workbenchSession.setPermissionCheckHandler(() => false);
  workbenchSession.setPermissionRequestHandler((_webContents, _permission, callback) => callback(false));
  registerAppProtocol(workbenchSession, join(__dirname, "../renderer"));

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
  if (smokeTest) {
    window.webContents.on("preload-error", (_event, _preloadPath, error) => {
      console.error(`Preload 加载失败：${error.message}`);
    });
  }
  installNavigationGuards(window.webContents);
  if (!smokeTest) window.once("ready-to-show", () => window.show());

  if (developmentUrl) {
    await window.loadURL(developmentUrl);
  } else {
    await window.loadURL("loop-app://app/");
  }
  return window;
}

function registerIpc(): void {
  ipcMain.handle(IPC_CHANNELS.getRuntimeInfo, (event): RuntimeInfo => {
    assertTrustedIpcSender(event, developmentUrl);
    return {
      appName: "Loop",
      appVersion: app.getVersion(),
      platform: platformName(),
      electronVersion: process.versions.electron,
      nativeService: nativeService.snapshot(),
    };
  });
}

app.whenReady().then(async () => {
  registerIpc();
  await nativeService.start();
  const window = await createWindow();
  if (smokeTest) {
    const runtime: unknown = await window.webContents.executeJavaScript(
      "typeof window.loop === 'object' ? window.loop.system.getRuntimeInfo() : ({ smoke_error: 'PRELOAD_BRIDGE_MISSING' })",
      true,
    );
    if (!isRuntimeInfo(runtime) || runtime.nativeService.status !== "ready") {
      const smokeError = typeof runtime === "object" && runtime !== null && "smoke_error" in runtime
        ? String(runtime.smoke_error)
        : "INVALID_RUNTIME_STATE";
      throw new Error(`桌面烟雾测试未通过：${smokeError}`);
    }
    app.exit(0);
    return;
  }

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) void createWindow();
  });
}).catch((error: unknown) => {
  if (smokeTest) {
    console.error(error instanceof Error ? error.message : "桌面烟雾测试失败");
  }
  app.exit(1);
});

app.on("before-quit", () => nativeService.stop());
app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});
