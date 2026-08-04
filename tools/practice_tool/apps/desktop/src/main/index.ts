import { join } from "node:path";
import { randomUUID } from "node:crypto";
import { appendFile, readFile, writeFile } from "node:fs/promises";
import {
  app,
  BrowserWindow,
  dialog,
  ipcMain,
  protocol,
  session,
  type WebContents,
} from "electron";
import { IPC_CHANNELS, is_runtime_info, type runtime_info } from "@loop/ipc-contracts";
import { native_service_supervisor } from "./native_service/native_service_supervisor";
import { register_app_protocol } from "./protocols/app_protocol";
import { register_preview_protocol } from "./protocols/preview_protocol";
import { PREVIEW_SCHEME_PRIVILEGES } from "./protocols/preview_protocol_policy.mts";
import { assert_trusted_ipc_sender, is_trusted_workbench_url } from "./security/trust_policy.mts";
import { workbench_controller } from "./workbench/workbench_controller.mts";
import { requires_close_confirmation } from "./workbench/window_close_policy.mts";
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
  {
    scheme: "loop-preview",
    privileges: PREVIEW_SCHEME_PRIVILEGES,
  },
]);
app.enableSandbox();

const development_url = app.isPackaged ? undefined : process.env.ELECTRON_RENDERER_URL;
const smoke_test = !app.isPackaged && process.env.LOOP_DESKTOP_SMOKE_TEST === "1";
const smoke_fixture = smoke_test ? process.env.LOOP_DESKTOP_SMOKE_FIXTURE ?? null : null;
const smoke_folder_fixture = smoke_test
  ? process.env.LOOP_DESKTOP_SMOKE_FOLDER_FIXTURE ?? null
  : null;
const smoke_hold_ms = smoke_test
  ? Math.min(60_000, Math.max(0, Number.parseInt(process.env.LOOP_DESKTOP_SMOKE_HOLD_MS ?? "0", 10) || 0))
  : 0;
let smoke_remote_request_count = 0;
const native_service = new native_service_supervisor();
const workbench = new workbench_controller(
  native_service,
  new electron_dialog_port(smoke_fixture, smoke_folder_fixture),
);

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
  if (smoke_test) {
    workbench_session.webRequest.onBeforeRequest({ urls: ["http://*/*", "https://*/*"] }, (_details, callback) => {
      smoke_remote_request_count += 1;
      callback({ cancel: true });
    });
  }
  register_app_protocol(workbench_session, join(__dirname, "../renderer"));
  register_preview_protocol(workbench_session, join(__dirname, "../renderer"));

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
  let confirmed_close = false;
  let close_prompt_pending = false;
  window.on("close", (event) => {
    if (confirmed_close) return;
    const dirty_count = workbench.dirty_count(web_contents_id);
    const dirty = dirty_count > 0;
    if (!requires_close_confirmation(workbench.has_workspace(web_contents_id), dirty_count)) return;
    event.preventDefault();
    if (close_prompt_pending) return;
    close_prompt_pending = true;
    void dialog.showMessageBox(window, {
      type: "warning",
      title: dirty ? "存在未保存修改" : "关闭当前工作区",
      message: dirty
        ? "修改目前只保存在内存中。关闭窗口将永久丢弃这些修改。"
        : "当前窗口仍打开一个工作区。请确认是否结束本次内存会话。",
      detail: dirty
        ? "保存已经开放，但 D2 尚未开放恢复备份或 Hot Exit；放弃后无法恢复。"
        : "在 Hot Exit 完成前，关闭工作区的窗口总是要求显式确认。",
      buttons: [dirty ? "放弃修改" : "关闭窗口", "取消"],
      defaultId: 1,
      cancelId: 1,
      noLink: true,
    }).then((result) => {
      close_prompt_pending = false;
      if (result.response === 0) {
        confirmed_close = true;
        window.close();
      }
    });
  });
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
  ipcMain.handle(IPC_CHANNELS.report_dirty_state, (event, request: unknown) => {
    assert_trusted_ipc_sender(event, development_url);
    return workbench.report_dirty_state(event.sender.id, request);
  });
  ipcMain.handle(IPC_CHANNELS.list_children, async (event, request: unknown) => {
    assert_trusted_ipc_sender(event, development_url);
    return workbench.list_children(event.sender.id, request);
  });
  ipcMain.handle(IPC_CHANNELS.open_document, async (event, request: unknown) => {
    assert_trusted_ipc_sender(event, development_url);
    return workbench.open_document(event.sender.id, request);
  });
  ipcMain.handle(IPC_CHANNELS.close_document, async (event, request: unknown) => {
    assert_trusted_ipc_sender(event, development_url);
    return workbench.close_document(event.sender.id, request);
  });
  ipcMain.handle(IPC_CHANNELS.save_document, async (event, request: unknown) => {
    assert_trusted_ipc_sender(event, development_url);
    return workbench.save_document(event.sender.id, request);
  });
}

app.whenReady().then(async () => {
  register_ipc();
  await native_service.start();
  const window = await create_window();
  if (smoke_test) {
    if (!smoke_fixture) throw new Error("桌面烟雾测试缺少 LOOP_DESKTOP_SMOKE_FIXTURE");
    const smoke_result: unknown = await window.webContents.executeJavaScript(
      `(async () => {
        if (typeof window.loop !== 'object'
          || typeof window.loop.system?.get_runtime_info !== 'function'
          || typeof window.loop.workbench?.open_file !== 'function'
          || typeof window.loop.workbench?.report_dirty_state !== 'function'
          || typeof window.loop.explorer?.list_children !== 'function'
          || typeof window.loop.documents?.open !== 'function'
          || typeof window.loop.documents?.save !== 'function'
          || typeof window.loop.documents?.close !== 'function') {
          return { smoke_error: 'PRELOAD_BRIDGE_MISSING' };
        }
        const runtime = await window.loop.system.get_runtime_info();
        const open_button = [...document.querySelectorAll('button')]
          .find((button) => button.textContent === ${smoke_folder_fixture ? "'打开文件夹'" : "'打开文件'"});
        if (!open_button) return { smoke_error: 'OPEN_BUTTON_MISSING' };
        open_button.click();
        const deadline = Date.now() + 10000;
        let folder_mode_visible = ${smoke_folder_fixture ? "false" : "true"};
        let tree_entry_visible = ${smoke_folder_fixture ? "false" : "true"};
        if (${smoke_folder_fixture ? "true" : "false"}) {
          while (Date.now() < deadline) {
            folder_mode_visible = document.body.innerText.includes('文件夹模式');
            const tree_entry = [...document.querySelectorAll('.tree_entry_row')]
              .find((entry) => entry.textContent?.includes('smoke.md'));
            tree_entry_visible = Boolean(tree_entry);
            if (folder_mode_visible && tree_entry) {
              tree_entry.dispatchEvent(new MouseEvent('click', { bubbles: true }));
              break;
            }
            await new Promise((resolve) => setTimeout(resolve, 25));
          }
        }
        let content = null;
        while (Date.now() < deadline) {
          content = document.querySelector('.cm-content');
          if (content?.textContent?.includes('LOOP_D1C_SMOKE_BODY')) break;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        content?.focus();
        document.execCommand('insertText', false, '\\nLOOP_D1C_SMOKE_EDIT');
        let dirty_visible = false;
        while (Date.now() < deadline) {
          dirty_visible = document.body.innerText.includes('未保存（仅内存）');
          if (dirty_visible) break;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        const save_event = new KeyboardEvent('keydown', { key: 's', ctrlKey: true, bubbles: true, cancelable: true });
        window.dispatchEvent(save_event);
        let save_succeeded = false;
        const save_deadline = Date.now() + 10000;
        while (Date.now() < save_deadline) {
          save_succeeded = save_event.defaultPrevented
            && document.body.innerText.includes('已安全保存到源文件')
            && !document.body.innerText.includes('未保存（仅内存）');
          if (save_succeeded) break;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        let preview_updated = false;
        let mermaid_frame = null;
        const preview_deadline = Date.now() + 10000;
        while (Date.now() < preview_deadline) {
          preview_updated = document.querySelectorAll('.loop_live_markdown_block').length > 0
            && document.querySelectorAll('.loop_mermaid_block:not(.is_loading):not(.has_error)').length === 3;
          mermaid_frame = document.querySelector('.loop_mermaid_frame');
          if (document.querySelector('.loop_mermaid_block.has_error')) break;
          if (preview_updated && mermaid_frame) break;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        const initial_theme = document.querySelector('.workbench')?.getAttribute('data-theme') ?? null;
        const theme_toggle = document.querySelector('[data-theme-toggle="true"]');
        theme_toggle?.click();
        let theme_toggled = false;
        let toggled_theme = null;
        const theme_deadline = Date.now() + 10000;
        while (Date.now() < theme_deadline) {
          toggled_theme = document.querySelector('.workbench')?.getAttribute('data-theme') ?? null;
          theme_toggled = (initial_theme === 'dark' || initial_theme === 'light')
            && (toggled_theme === 'dark' || toggled_theme === 'light')
            && toggled_theme !== initial_theme
            && document.querySelectorAll('.loop_mermaid_block:not(.is_loading):not(.has_error)').length === 3;
          if (theme_toggled) break;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        const source_event = new KeyboardEvent('keydown', { key: '/', code: 'Slash', ctrlKey: true, bubbles: true, cancelable: true });
        window.dispatchEvent(source_event);
        let source_mode_toggled = false;
        const mode_deadline = Date.now() + 5000;
        while (Date.now() < mode_deadline) {
          source_mode_toggled = source_event.defaultPrevented
            && document.querySelector('.editor_stack')?.getAttribute('data-source-mode') === 'source';
          if (source_mode_toggled) break;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        const hybrid_event = new KeyboardEvent('keydown', { key: '/', code: 'Slash', ctrlKey: true, bubbles: true, cancelable: true });
        window.dispatchEvent(hybrid_event);
        const hybrid_deadline = Date.now() + 10000;
        while (Date.now() < hybrid_deadline) {
          if (document.querySelector('.editor_stack')?.getAttribute('data-source-mode') === 'hybrid'
              && document.querySelectorAll('.loop_mermaid_block:not(.is_loading):not(.has_error)').length === 3) break;
          if (document.querySelector('.loop_mermaid_block.has_error')) break;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        preview_updated = document.querySelectorAll('.loop_live_markdown_block').length > 0
          && document.querySelectorAll('.loop_mermaid_block:not(.is_loading):not(.has_error)').length === 3;
        mermaid_frame = document.querySelector('.loop_mermaid_frame');
        const rendered_text = document.querySelector('.cm-content')?.textContent ?? '';
        const legacy_mode_buttons_absent = ![...document.querySelectorAll('button')]
          .some((button) => ['仅源码', '侧边预览', '仅预览'].includes(button.textContent ?? ''));
        await new Promise((resolve) => setTimeout(resolve, ${smoke_hold_ms}));
        return {
          runtime,
          folder_mode_visible,
          tree_entry_visible,
          editor_loaded: Boolean(content?.textContent?.includes('LOOP_D1C_SMOKE_BODY')),
          dirty_visible,
          save_succeeded,
          preview_updated,
          rendered_text,
          source_mode_toggled,
          initial_theme,
          toggled_theme,
          theme_toggled,
          hybrid_mode_restored: document.querySelector('.editor_stack')?.getAttribute('data-source-mode') === 'hybrid',
          legacy_mode_buttons_absent,
          mermaid_frame_sandbox: mermaid_frame?.getAttribute('sandbox') ?? null,
          mermaid_frame_source: mermaid_frame?.getAttribute('src') ?? null,
          mermaid_frame_content_accessible: Boolean(mermaid_frame?.contentDocument),
          mermaid_frame_count: document.querySelectorAll('.loop_mermaid_frame').length,
          mermaid_errors: [...document.querySelectorAll('.loop_mermaid_block.has_error')]
            .map((block) => String(block.getAttribute('data-block-id')) + ':'
              + String(block.getAttribute('data-mermaid-error')) + ':'
              + String(block.getAttribute('data-mermaid-error-detail'))),
          mermaid_states: [...document.querySelectorAll('.loop_mermaid_block')]
            .map((block) => block.className),
          workbench_script_executed: globalThis.LOOP_PREVIEW_SCRIPT_EXECUTED === true,
          absolute_path_visible: /(?:[A-Za-z]:\\\\|\\\\\\\\[^\\s]+\\\\[^\\s]+)/.test(document.body.innerText),
        };
      })()`,
      true,
    );
    const smoke_target = smoke_folder_fixture ? join(smoke_folder_fixture, "smoke.md") : smoke_fixture;
    const saved_disk_source = await readFile(smoke_target, "utf8");
    await appendFile(smoke_target, "\nLOOP_D1S_EXTERNAL_CHANGE\n", "utf8");
    const conflict_probe: unknown = await window.webContents.executeJavaScript(
      `(async () => {
        const content = document.querySelector('.cm-content');
        content?.focus();
        document.execCommand('insertText', false, '\\nLOOP_D1S_SECOND_EDIT');
        const deadline = Date.now() + 10000;
        while (Date.now() < deadline && !document.body.innerText.includes('未保存（仅内存）')) {
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        const save_event = new KeyboardEvent('keydown', { key: 's', ctrlKey: true, bubbles: true, cancelable: true });
        window.dispatchEvent(save_event);
        let conflict_reported = false;
        while (Date.now() < deadline) {
          conflict_reported = document.body.innerText.includes('磁盘文件已经变化，未覆盖外部内容');
          if (conflict_reported) break;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
        return { conflict_reported, dirty_retained: document.body.innerText.includes('未保存（仅内存）') };
      })()`,
      true,
    );
    const conflicted_disk_source = await readFile(smoke_target, "utf8");
    type smoke_preview_candidate = {
      diagram_text: string;
      svg_rendered: boolean;
      frame_origin: string;
      runtime_ready: boolean;
      controls_ready: boolean;
      preload_exposed: boolean;
      script_executed: boolean;
      theme: string | null;
      zoom_before: string | null;
      zoom_after: string | null;
      fit_width: string | null;
      zoom_maximum: string | null;
      zoom_reset: string | null;
      zoom_maximum_disabled: boolean;
      local_overflow: boolean;
      keyboard_zoom: string | null;
      keyboard_reset: string | null;
      zoom_minimum: string | null;
      zoom_minimum_disabled: boolean;
    };
    let preview_probe: unknown = null;
    const rendered_candidates = new Map<string, smoke_preview_candidate>();
    const zoom_verified_candidates: smoke_preview_candidate[] = [];
    let observed_preload = false;
    let observed_script = false;
    let observed_untrusted_origin = false;
    let preview_frame_timeout_count = 0;
    const preview_candidate_observations: string[] = [];
    let probe_iteration = 0;
    const preview_probe_deadline = Date.now() + 30_000;
    while (Date.now() < preview_probe_deadline) {
      await window.webContents.executeJavaScript(
        `(() => {
          const scroller = document.querySelector('.cm-scroller');
          if (!(scroller instanceof HTMLElement)) return;
          const maximum = Math.max(0, scroller.scrollHeight - scroller.clientHeight);
          const positions = [0, 0.15, 0.3, 0.45, 0.6, 0.75, 0.9, 1]
            .map((ratio) => Math.round(maximum * ratio));
          scroller.scrollTop = positions[${probe_iteration} % positions.length] ?? 0;
        })()`,
        true,
      );
      // CodeMirror 会销毁视口外的 Frame；给新进入视口的 3.4 MiB 隔离 runtime
      // 留出完成加载和握手的时间，避免探针在下一次滚动前反复中断它。
      await new Promise((resolve) => setTimeout(resolve, 600));
      const preview_frames = window.webContents.mainFrame.frames
        .filter((frame) => !frame.detached && frame.url.startsWith("loop-preview://preview/"));
      for (const preview_frame of preview_frames) {
        try {
          const candidate = await Promise.race<smoke_preview_candidate | null>([
            preview_frame.executeJavaScript(`(() => {
            const zoom_value = document.querySelector('#mermaid_zoom_value');
            const zoom_out = document.querySelector('[data-mermaid-action="zoom_out"]');
            const zoom_in = document.querySelector('[data-mermaid-action="zoom_in"]');
            const reset = document.querySelector('[data-mermaid-action="reset"]');
            const fit = document.querySelector('[data-mermaid-action="fit"]');
            const viewport = document.querySelector('#mermaid_viewport');
            const zoom_before = zoom_value?.textContent ?? null;
            zoom_in?.click();
            const zoom_after = zoom_value?.textContent ?? null;
            fit?.click();
            const fit_width = zoom_value?.textContent ?? null;
            for (let index = 0; index < 8; index += 1) zoom_in?.click();
            const zoom_maximum = zoom_value?.textContent ?? null;
            const zoom_maximum_disabled = zoom_in instanceof HTMLButtonElement && zoom_in.disabled;
            const svg = document.querySelector('svg');
            const local_overflow = viewport instanceof HTMLElement
              && svg instanceof SVGElement && svg.style.width === '300%'
              && getComputedStyle(viewport).overflowX === 'auto'
              && getComputedStyle(document.documentElement).overflowX === 'hidden';
            reset?.click();
            window.dispatchEvent(new KeyboardEvent('keydown', { key: '=', code: 'Equal', ctrlKey: true, bubbles: true, cancelable: true }));
            const keyboard_zoom = zoom_value?.textContent ?? null;
            window.dispatchEvent(new KeyboardEvent('keydown', { key: '0', code: 'Digit0', ctrlKey: true, bubbles: true, cancelable: true }));
            const keyboard_reset = zoom_value?.textContent ?? null;
            zoom_out?.click();
            zoom_out?.click();
            const zoom_minimum = zoom_value?.textContent ?? null;
            const zoom_minimum_disabled = zoom_out instanceof HTMLButtonElement && zoom_out.disabled;
            reset?.click();
            return {
              diagram_text: [...document.querySelectorAll('svg text')].map((node) => node.textContent ?? '').join(' '),
              svg_rendered: Boolean(document.querySelector('svg')),
              frame_origin: location.origin,
              runtime_ready: document.documentElement.dataset.previewRuntime === 'ready',
              controls_ready: document.documentElement.dataset.previewControls === 'ready',
              preload_exposed: typeof window.loop !== 'undefined',
              script_executed: globalThis.LOOP_PREVIEW_SCRIPT_EXECUTED === true,
              theme: document.documentElement.dataset.theme ?? null,
              zoom_before,
              zoom_after,
              fit_width,
              zoom_maximum,
              zoom_reset: zoom_value?.textContent ?? null,
              zoom_maximum_disabled,
              local_overflow,
              keyboard_zoom,
              keyboard_reset,
              zoom_minimum,
              zoom_minimum_disabled,
            };
            })()`, true) as Promise<smoke_preview_candidate>,
            new Promise<null>((resolve) => setTimeout(() => resolve(null), 1_200)),
          ]);
          if (!candidate) {
            preview_frame_timeout_count += 1;
            continue;
          }
          preview_candidate_observations.push(candidate.diagram_text);
          observed_preload ||= candidate.preload_exposed;
          observed_script ||= candidate.script_executed;
          observed_untrusted_origin ||= candidate.frame_origin !== "loop-preview://preview";
          const candidate_verified = candidate.svg_rendered && candidate.runtime_ready && candidate.controls_ready
            && candidate.zoom_before === "100%" && candidate.zoom_after === "125%"
            && candidate.fit_width === "100%" && candidate.zoom_maximum === "300%" && candidate.zoom_reset === "100%"
            && candidate.zoom_maximum_disabled && candidate.local_overflow
            && candidate.keyboard_zoom === "125%" && candidate.keyboard_reset === "100%"
            && candidate.zoom_minimum === "50%" && candidate.zoom_minimum_disabled;
          const diagram_key = candidate.diagram_text.includes("本地编辑") && candidate.diagram_text.includes("及时渲染")
            ? "flowchart"
            : candidate.diagram_text.includes("回路") && candidate.diagram_text.includes("用户")
                && candidate.diagram_text.includes("编辑 Markdown") && candidate.diagram_text.includes("返回及时渲染")
              ? "sequence"
              : candidate.diagram_text.includes("保存后继续渲染")
                && candidate.diagram_text.includes("Editing") && candidate.diagram_text.includes("Rendered")
                ? "state"
                : null;
          if (diagram_key && candidate.svg_rendered && candidate.runtime_ready && candidate.controls_ready) {
            rendered_candidates.set(diagram_key, candidate);
          }
          if (candidate_verified) zoom_verified_candidates.push(candidate);
        } catch (error: unknown) {
          preview_probe = { preview_error: error instanceof Error ? error.message : "PREVIEW_PROBE_FAILED" };
        }
      }
      if (rendered_candidates.size === 3 && zoom_verified_candidates.length > 0) break;
      probe_iteration += 1;
      await new Promise((resolve) => setTimeout(resolve, 25));
    }
    const rendered_values = [...rendered_candidates.values()];
    preview_probe = {
      diagram_text: rendered_values.map((candidate) => candidate.diagram_text).join(" "),
      svg_rendered: rendered_candidates.size === 3,
      frame_origin: observed_untrusted_origin ? "unexpected" : "loop-preview://preview",
      runtime_ready: rendered_candidates.size === 3,
      controls_ready: rendered_candidates.size === 3,
      preload_exposed: observed_preload,
      script_executed: observed_script,
      themes: rendered_values.map((candidate) => candidate.theme),
      zoom_details: zoom_verified_candidates.slice(0, 3).map((candidate) => ({
        zoom_before: candidate.zoom_before,
        zoom_after: candidate.zoom_after,
        fit_width: candidate.fit_width,
        zoom_maximum: candidate.zoom_maximum,
        zoom_reset: candidate.zoom_reset,
        zoom_maximum_disabled: candidate.zoom_maximum_disabled,
        local_overflow: candidate.local_overflow,
        keyboard_zoom: candidate.keyboard_zoom,
        keyboard_reset: candidate.keyboard_reset,
        zoom_minimum: candidate.zoom_minimum,
        zoom_minimum_disabled: candidate.zoom_minimum_disabled,
      })),
      zoom_verified: zoom_verified_candidates.length > 0,
      preview_frame_timeout_count,
      preview_candidate_observations,
    };
    const valid_result = typeof smoke_result === "object" && smoke_result !== null
      && "runtime" in smoke_result
      && is_runtime_info(smoke_result.runtime)
      && smoke_result.runtime.native_service.status === "ready"
      && "folder_mode_visible" in smoke_result && smoke_result.folder_mode_visible === true
      && "tree_entry_visible" in smoke_result && smoke_result.tree_entry_visible === true
      && "editor_loaded" in smoke_result && smoke_result.editor_loaded === true
      && "dirty_visible" in smoke_result && smoke_result.dirty_visible === true
      && "save_succeeded" in smoke_result && smoke_result.save_succeeded === true
      && "preview_updated" in smoke_result && smoke_result.preview_updated === true
      && "rendered_text" in smoke_result && typeof smoke_result.rendered_text === "string"
      && smoke_result.rendered_text.includes("LOOP_D1C_SMOKE_BODY")
      && smoke_result.rendered_text.includes("Worker 实体解码：©")
      && smoke_result.rendered_text.includes("资源加载已阻止")
      && "source_mode_toggled" in smoke_result && smoke_result.source_mode_toggled === true
      && "theme_toggled" in smoke_result && smoke_result.theme_toggled === true
      && "initial_theme" in smoke_result && (smoke_result.initial_theme === "dark" || smoke_result.initial_theme === "light")
      && "toggled_theme" in smoke_result && (smoke_result.toggled_theme === "dark" || smoke_result.toggled_theme === "light")
      && smoke_result.initial_theme !== smoke_result.toggled_theme
      && "hybrid_mode_restored" in smoke_result && smoke_result.hybrid_mode_restored === true
      && "legacy_mode_buttons_absent" in smoke_result && smoke_result.legacy_mode_buttons_absent === true
      && "mermaid_frame_sandbox" in smoke_result && smoke_result.mermaid_frame_sandbox === "allow-scripts"
      && "mermaid_frame_source" in smoke_result && smoke_result.mermaid_frame_source === "loop-preview://preview/"
      && "mermaid_frame_content_accessible" in smoke_result && smoke_result.mermaid_frame_content_accessible === false
      && "mermaid_frame_count" in smoke_result && smoke_result.mermaid_frame_count === 3
      && "mermaid_errors" in smoke_result && Array.isArray(smoke_result.mermaid_errors)
      && smoke_result.mermaid_errors.length === 0
      && "workbench_script_executed" in smoke_result && smoke_result.workbench_script_executed === false
      && "absolute_path_visible" in smoke_result && smoke_result.absolute_path_visible === false
      && typeof preview_probe === "object" && preview_probe !== null
      && "diagram_text" in preview_probe && typeof preview_probe.diagram_text === "string"
      && preview_probe.diagram_text.includes("本地编辑")
      && preview_probe.diagram_text.includes("及时渲染")
      && preview_probe.diagram_text.includes("编辑 Markdown")
      && preview_probe.diagram_text.includes("返回及时渲染")
      && preview_probe.diagram_text.includes("保存后继续渲染")
      && "svg_rendered" in preview_probe && preview_probe.svg_rendered === true
      && "runtime_ready" in preview_probe && preview_probe.runtime_ready === true
      && "controls_ready" in preview_probe && preview_probe.controls_ready === true
      && "preload_exposed" in preview_probe && preview_probe.preload_exposed === false
      && "script_executed" in preview_probe && preview_probe.script_executed === false
      && "themes" in preview_probe && Array.isArray(preview_probe.themes)
      && preview_probe.themes.length === 3
      && preview_probe.themes.every((theme) => theme === smoke_result.toggled_theme)
      && "zoom_verified" in preview_probe && preview_probe.zoom_verified === true
      && saved_disk_source.includes("LOOP_D1C_SMOKE_EDIT")
      && typeof conflict_probe === "object" && conflict_probe !== null
      && "conflict_reported" in conflict_probe && conflict_probe.conflict_reported === true
      && "dirty_retained" in conflict_probe && conflict_probe.dirty_retained === true
      && conflicted_disk_source.includes("LOOP_D1S_EXTERNAL_CHANGE")
      && !conflicted_disk_source.includes("LOOP_D1S_SECOND_EDIT")
      && smoke_remote_request_count === 0;
    if (!valid_result) {
      const diagnostic = JSON.stringify({
        smoke_result,
        preview_probe,
        conflict_probe,
        saved_disk_has_edit: saved_disk_source.includes("LOOP_D1C_SMOKE_EDIT"),
        conflicted_disk_has_external_change: conflicted_disk_source.includes("LOOP_D1S_EXTERNAL_CHANGE"),
        conflicted_disk_has_second_edit: conflicted_disk_source.includes("LOOP_D1S_SECOND_EDIT"),
        smoke_remote_request_count,
      });
      console.error(diagnostic);
      await writeFile(`${smoke_target}.diagnostic.json`, diagnostic, "utf8");
      const smoke_error = typeof smoke_result === "object" && smoke_result !== null && "smoke_error" in smoke_result
        ? String(smoke_result.smoke_error)
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

app.on("will-quit", () => native_service.stop());
app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});
