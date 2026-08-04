import { useEffect, useMemo, useRef, useState } from "react";
import {
  begin_document_save,
  complete_document_save,
  create_document_session,
  fail_document_save,
  record_document_change,
  type document_session_state,
} from "@loop/document-core";
import { PREVIEW_PROTOCOL_VERSION, type preview_document } from "@loop/markdown-engine/contracts";
import type {
  command_result,
  document_snapshot,
  document_target_kind,
  file_entry,
  opened_folder,
  opened_single_file,
  runtime_info,
  line_ending_policy,
} from "@loop/ipc-contracts";
import {
  DocumentEditor,
  type document_editor_handle,
  type editor_position,
} from "./editor/document_editor";
import { add_document_tab } from "./editor/document_tab_model.mts";
import { ExplorerTree } from "./explorer/explorer_tree";
import { preview_coordinator } from "./preview/preview_coordinator.mts";
import { system_workbench_theme, type workbench_theme } from "./theme/workbench_theme.mts";

type opened_workspace = opened_single_file | opened_folder;

interface document_tab {
  snapshot: document_snapshot;
  session: document_session_state;
}

const MAXIMUM_OPEN_TABS = 32;

function result_error<value_type>(result: command_result<value_type>): string | null {
  return result.status === "error" ? result.error.user_message : null;
}

export function App() {
  const [runtime, set_runtime] = useState<runtime_info | null>(null);
  const [workspace, set_workspace] = useState<opened_workspace | null>(null);
  const [tabs, set_tabs] = useState<document_tab[]>([]);
  const [active_document_id, set_active_document_id] = useState<string | null>(null);
  const [positions, set_positions] = useState<Record<string, editor_position>>({});
  const [preview_documents, set_preview_documents] = useState<Record<string, preview_document>>({});
  const [preview_failures, set_preview_failures] = useState<Record<string, string>>({});
  const [source_mode, set_source_mode] = useState(false);
  const [theme, set_theme] = useState<workbench_theme>(system_workbench_theme);
  const [busy, set_busy] = useState(false);
  const [message, set_message] = useState<string | null>(null);
  const [format_decision_document_id, set_format_decision_document_id] = useState<string | null>(null);
  const [close_decision_document_id, set_close_decision_document_id] = useState<string | null>(null);
  const [closing_document_id, set_closing_document_id] = useState<string | null>(null);
  const workspace_generation_ref = useRef(0);
  const opening_targets_ref = useRef(new Set<string>());
  const editor_handles_ref = useRef(new Map<string, document_editor_handle>());
  const destructive_operation_ref = useRef(false);
  const tabs_ref = useRef<document_tab[]>([]);
  const preview_coordinator_ref = useRef<preview_coordinator | null>(null);
  const close_after_save_document_id_ref = useRef<string | null>(null);

  const active_tab = useMemo(
    () => tabs.find((tab) => tab.snapshot.document_id === active_document_id) ?? null,
    [active_document_id, tabs],
  );

  function toggle_source_mode(): void {
    set_source_mode((current) => {
      const next = !current;
      set_message(next ? "已进入完整源码模式；按 Ctrl+/ 返回混合编辑" : "已返回混合编辑模式");
      return next;
    });
  }

  useEffect(() => {
    let active = true;
    window.loop.system.get_runtime_info().then(
      (value) => {
        if (active) set_runtime(value);
      },
      () => {
        if (active) set_message("无法取得桌面运行时状态");
      },
    );
    return () => {
      active = false;
    };
  }, []);

  useEffect(() => {
    const coordinator = new preview_coordinator(
      () => new Worker(new URL("./preview/preview_worker.ts", import.meta.url), {
        type: "module",
        name: "loop-markdown-preview",
      }),
      {
        on_result: (document) => {
          set_preview_documents((current) => ({ ...current, [document.document_id]: document }));
          set_preview_failures((current) => {
            if (!(document.document_id in current)) return current;
            const next = { ...current };
            delete next[document.document_id];
            return next;
          });
        },
        on_failure: (failure) => {
          set_preview_failures((current) => ({
            ...current,
            [failure.document_id]: failure.user_message,
          }));
          set_message(failure.user_message);
        },
      },
    );
    preview_coordinator_ref.current = coordinator;
    return () => {
      preview_coordinator_ref.current = null;
      coordinator.dispose();
    };
  }, []);

  useEffect(() => {
    const on_key_down = (event: KeyboardEvent): void => {
      if (event.defaultPrevented) return;
      const key = event.key.toLowerCase();
      if (!(event.ctrlKey || event.metaKey)) return;
      if (!event.altKey && key === "s") {
        event.preventDefault();
        if (active_document_id) void save_document(active_document_id);
        return;
      }
      if (!event.altKey && !event.shiftKey && (event.key === "/" || event.code === "Slash")) {
        event.preventDefault();
        toggle_source_mode();
        return;
      }
    };
    window.addEventListener("keydown", on_key_down);
    return () => window.removeEventListener("keydown", on_key_down);
  }, [active_document_id, workspace]);

  function replace_workspace(value: opened_workspace): number {
    const generation = ++workspace_generation_ref.current;
    opening_targets_ref.current.clear();
    editor_handles_ref.current.clear();
    preview_coordinator_ref.current?.clear_all();
    tabs_ref.current = [];
    set_workspace(value);
    set_tabs([]);
    set_active_document_id(null);
    set_positions({});
    set_preview_documents({});
    set_preview_failures({});
    set_format_decision_document_id(null);
    set_close_decision_document_id(null);
    close_after_save_document_id_ref.current = null;
    return generation;
  }

  function set_destructive_operation(active: boolean): void {
    destructive_operation_ref.current = active;
    for (const handle of editor_handles_ref.current.values()) handle.set_editable(!active);
    set_busy(active);
  }

  async function synchronize_dirty_state(current_workspace: opened_workspace | null): Promise<boolean> {
    if (!current_workspace) return true;
    const dirty_count = tabs_ref.current.reduce((count, tab) => count + (tab.session.dirty ? 1 : 0), 0);
    try {
      const result = await window.loop.workbench.report_dirty_state({
        workspace_id: current_workspace.workspace_id,
        dirty_count,
      });
      const error = result_error(result);
      if (error) set_message(error);
      return result.status === "ok";
    } catch {
      set_message("无法同步窗口 Dirty 状态，已取消破坏性操作");
      return false;
    }
  }

  async function open_document(
    current_workspace: opened_workspace,
    target_kind: document_target_kind,
    target_id: string,
    expected_generation = workspace_generation_ref.current,
  ): Promise<void> {
    const target_key = `${target_kind}:${target_id}`;
    if (opening_targets_ref.current.has(target_key)) return;
    opening_targets_ref.current.add(target_key);
    try {
      const result = await window.loop.documents.open({
        workspace_id: current_workspace.workspace_id,
        target_kind,
        target_id,
      });
      if (expected_generation !== workspace_generation_ref.current) return;
      const error = result_error(result);
      if (error) {
        set_message(error);
        return;
      }
      if (result.status !== "ok") return;
      const addition = add_document_tab(tabs_ref.current, {
        snapshot: result.value,
        session: create_document_session(result.value.document_id),
      }, MAXIMUM_OPEN_TABS);
      if (addition.status === "limit") {
        set_message(`最多同时打开 ${MAXIMUM_OPEN_TABS} 个文档`);
        return;
      }
      tabs_ref.current = addition.tabs;
      set_tabs(addition.tabs);
      set_active_document_id(result.value.document_id);
      set_positions((current) => ({
        ...current,
        [result.value.document_id]: current[result.value.document_id] ?? { line: 1, column: 1 },
      }));
    } catch {
      if (expected_generation === workspace_generation_ref.current) set_message("正文打开请求失败");
    } finally {
      opening_targets_ref.current.delete(target_key);
    }
  }

  async function open_file(): Promise<void> {
    set_destructive_operation(true);
    set_message(null);
    try {
      if (!await synchronize_dirty_state(workspace)) return;
      const result = await window.loop.workbench.open_file();
      const error = result_error(result);
      if (error) set_message(error);
      if (result.status === "ok") {
        const generation = replace_workspace(result.value);
        await open_document(result.value, "document", result.value.document.document_id, generation);
      }
    } catch {
      set_message("打开文件请求失败");
    } finally {
      set_destructive_operation(false);
    }
  }

  async function open_folder(): Promise<void> {
    set_destructive_operation(true);
    set_message(null);
    try {
      if (!await synchronize_dirty_state(workspace)) return;
      const result = await window.loop.workbench.open_folder();
      const error = result_error(result);
      if (error) set_message(error);
      if (result.status === "ok") replace_workspace(result.value);
    } catch {
      set_message("打开文件夹请求失败");
    } finally {
      set_destructive_operation(false);
    }
  }

  async function close_workspace(): Promise<void> {
    set_destructive_operation(true);
    set_message(null);
    try {
      if (!await synchronize_dirty_state(workspace)) return;
      const result = await window.loop.workbench.close_workspace();
      const error = result_error(result);
      if (error) set_message(error);
      if (result.status === "ok") {
        ++workspace_generation_ref.current;
        set_workspace(null);
        tabs_ref.current = [];
        set_tabs([]);
        set_active_document_id(null);
        set_positions({});
        preview_coordinator_ref.current?.clear_all();
        set_preview_documents({});
        set_preview_failures({});
      }
    } catch {
      set_message("关闭工作区请求失败");
    } finally {
      set_destructive_operation(false);
    }
  }

  function open_folder_entry(entry: file_entry): void {
    if (!workspace || workspace.mode !== "folder") return;
    set_message(null);
    void open_document(workspace, "entry", entry.entry_id);
  }

  function update_document_state(document_id: string, content_equals_baseline: boolean): number {
    let editor_revision: number | null = null;
    const updated = tabs_ref.current.map((tab) => {
      if (tab.snapshot.document_id !== document_id) return tab;
      const session = record_document_change(tab.session, content_equals_baseline);
      editor_revision = session.editor_revision;
      return { ...tab, session };
    });
    if (editor_revision === null) throw new Error("编辑事务对应的文档会话不存在");
    tabs_ref.current = updated;
    set_tabs(updated);
    if (workspace) {
      const next_dirty_count = updated.reduce((count, tab) => count + (tab.session.dirty ? 1 : 0), 0);
      void window.loop.workbench.report_dirty_state({
        workspace_id: workspace.workspace_id,
        dirty_count: next_dirty_count,
      }).then((result) => {
        if (result.status === "error") set_message(result.error.user_message);
      }).catch(() => set_message("无法同步窗口 Dirty 状态"));
    }
    return editor_revision;
  }

  function update_preview_source(document_id: string, revision: number, source: string): void {
    set_preview_failures((current) => {
      if (!(document_id in current)) return current;
      const next = { ...current };
      delete next[document_id];
      return next;
    });
    preview_coordinator_ref.current?.submit({
      message_type: "parse_preview",
      protocol_version: PREVIEW_PROTOCOL_VERSION,
      document_id,
      revision,
      source,
    });
  }

  function update_position(document_id: string, position: editor_position): void {
    set_positions((current) => ({ ...current, [document_id]: position }));
  }

  function publish_tabs(next_tabs: document_tab[]): void {
    tabs_ref.current = next_tabs;
    set_tabs(next_tabs);
    if (!workspace) return;
    const dirty_count = next_tabs.reduce((count, tab) => count + (tab.session.dirty ? 1 : 0), 0);
    void window.loop.workbench.report_dirty_state({
      workspace_id: workspace.workspace_id,
      dirty_count,
    }).then((result) => {
      if (result.status === "error") set_message(result.error.user_message);
    }).catch(() => set_message("无法同步窗口 Dirty 状态"));
  }

  async function save_document(
    document_id: string,
    line_ending_policy: line_ending_policy = "preserve",
  ): Promise<boolean> {
    const current_workspace = workspace;
    const generation = workspace_generation_ref.current;
    const tab = tabs_ref.current.find((candidate) => candidate.snapshot.document_id === document_id);
    const handle = editor_handles_ref.current.get(document_id);
    if (!current_workspace || !tab || !handle) return false;
    if (tab.session.saving_revision !== null) {
      set_message("该文档正在保存，请等待当前保存完成");
      return false;
    }
    if (tab.snapshot.read_only) {
      set_message("文件是只读的，未执行保存");
      return false;
    }
    if (!tab.session.dirty) {
      set_message("文档内容与已保存基线一致");
      return true;
    }
    if (tab.snapshot.line_ending === "mixed" && line_ending_policy === "preserve") {
      set_format_decision_document_id(document_id);
      set_message("文件包含混合换行；请选择统一为 LF 或 CRLF 后再保存");
      return false;
    }

    const prepared = handle.prepare_save();
    const saving_tabs = tabs_ref.current.map((candidate) => candidate.snapshot.document_id === document_id
      ? { ...candidate, session: begin_document_save(candidate.session, prepared.editor_revision) }
      : candidate);
    publish_tabs(saving_tabs);
    set_format_decision_document_id(null);
    set_message("正在安全保存…");
    try {
      const result = await window.loop.documents.save({
        workspace_id: current_workspace.workspace_id,
        document_id,
        expected_file_version_token: tab.snapshot.file_version_token,
        expected_content_hash: tab.snapshot.content_hash,
        editor_revision: prepared.editor_revision,
        line_ending_policy,
        content: prepared.content,
      });
      if (generation !== workspace_generation_ref.current) return false;
      if (result.status !== "ok") {
        handle.reject_saved(prepared.editor_revision);
        const failed_tabs = tabs_ref.current.map((candidate) => candidate.snapshot.document_id === document_id
          ? { ...candidate, session: fail_document_save(candidate.session, prepared.editor_revision) }
          : candidate);
        publish_tabs(failed_tabs);
        if (result.status === "error" && result.error.code === "FORMAT_DECISION_REQUIRED") {
          set_format_decision_document_id(document_id);
        }
        set_message(result.status === "error" ? result.error.user_message : "保存已取消");
        return false;
      }
      const content_equals_saved_baseline = handle.accept_saved(result.value.saved_revision);
      const saved_tabs = tabs_ref.current.map((candidate) => {
        if (candidate.snapshot.document_id !== document_id) return candidate;
        return {
          ...candidate,
          snapshot: {
            ...candidate.snapshot,
            content: result.value.line_ending === "crlf"
              ? prepared.content.replaceAll("\n", "\r\n")
              : prepared.content,
            content_hash: result.value.content_hash,
            file_version_token: result.value.file_version_token,
            byte_size: result.value.byte_size,
            modified_time_ms: result.value.modified_time_ms,
            encoding: result.value.encoding,
            bom: result.value.bom,
            line_ending: result.value.line_ending,
            read_only: result.value.read_only,
            resolved_from_link: result.value.resolved_from_link,
          },
          session: complete_document_save(
            candidate.session,
            result.value.saved_revision,
            content_equals_saved_baseline,
          ),
        };
      });
      publish_tabs(saved_tabs);
      set_message(content_equals_saved_baseline
        ? "已安全保存到源文件"
        : "修订已保存；保存期间产生的新修改仍未保存");
      if (content_equals_saved_baseline
          && close_after_save_document_id_ref.current === document_id) {
        close_after_save_document_id_ref.current = null;
        set_close_decision_document_id(null);
        await close_document_tab(document_id, true);
      }
      return content_equals_saved_baseline;
    } catch {
      handle.reject_saved(prepared.editor_revision);
      if (generation !== workspace_generation_ref.current) return false;
      const failed_tabs = tabs_ref.current.map((candidate) => candidate.snapshot.document_id === document_id
        ? { ...candidate, session: fail_document_save(candidate.session, prepared.editor_revision) }
        : candidate);
      publish_tabs(failed_tabs);
      set_message("保存请求失败；磁盘是否变更未得到确认，请重新打开文件核对");
      return false;
    }
  }

  async function close_document_tab(document_id: string, discard_dirty = false): Promise<void> {
    const current_workspace = workspace;
    const tab = tabs_ref.current.find((candidate) => candidate.snapshot.document_id === document_id);
    if (!current_workspace || current_workspace.mode !== "folder" || !tab) return;
    if (tab.session.saving_revision !== null) {
      set_message("文档正在保存，当前不能关闭标签");
      return;
    }
    if (tab.session.dirty && !discard_dirty) {
      set_close_decision_document_id(document_id);
      return;
    }
    set_closing_document_id(document_id);
    editor_handles_ref.current.get(document_id)?.set_editable(false);
    try {
      const result = await window.loop.documents.close({
        workspace_id: current_workspace.workspace_id,
        document_id,
      });
      if (result.status !== "ok") {
        editor_handles_ref.current.get(document_id)?.set_editable(true);
        set_message(result.status === "error" ? result.error.user_message : "关闭文档已取消");
        return;
      }
      const previous_tabs = tabs_ref.current;
      const closed_index = previous_tabs.findIndex((candidate) => candidate.snapshot.document_id === document_id);
      const next_tabs = previous_tabs.filter((candidate) => candidate.snapshot.document_id !== document_id);
      editor_handles_ref.current.delete(document_id);
      preview_coordinator_ref.current?.clear_document(document_id);
      set_preview_documents((current) => {
        const next = { ...current };
        delete next[document_id];
        return next;
      });
      set_preview_failures((current) => {
        const next = { ...current };
        delete next[document_id];
        return next;
      });
      set_positions((current) => {
        const next = { ...current };
        delete next[document_id];
        return next;
      });
      publish_tabs(next_tabs);
      set_active_document_id((current) => current === document_id
        ? next_tabs[Math.max(0, Math.min(closed_index, next_tabs.length - 1))]?.snapshot.document_id ?? null
        : current);
      set_close_decision_document_id(null);
      set_format_decision_document_id(null);
      set_message(tab.session.dirty ? "已明确放弃内存修改并关闭标签" : "文档标签已关闭");
    } catch {
      editor_handles_ref.current.get(document_id)?.set_editable(true);
      set_message("关闭文档请求失败；标签和内存草稿仍保留");
    } finally {
      set_closing_document_id(null);
    }
  }

  const active_position = active_document_id ? positions[active_document_id] ?? { line: 1, column: 1 } : null;
  return (
    <main className="workbench" data-theme={theme}>
      <header className="titlebar">
        <span className="product_mark" aria-hidden="true">◉</span>
        <span className="window_title">回路（Loop）Markdown 工作台</span>
        <span className="workspace_title">{workspace?.display_name ?? "空窗口"}</span>
      </header>

      <nav className="commandbar" aria-label="工作台命令">
        <button type="button" onClick={() => void open_file()} disabled={busy}>打开文件</button>
        <button type="button" onClick={() => void open_folder()} disabled={busy}>打开文件夹</button>
        {workspace && <button type="button" className="quiet" onClick={() => void close_workspace()} disabled={busy}>关闭工作区</button>}
        <span className="command_separator" aria-hidden="true" />
        <button type="button" className="quiet" disabled={!active_tab || busy} onClick={() => {
          if (!destructive_operation_ref.current && active_document_id) editor_handles_ref.current.get(active_document_id)?.undo();
        }}>撤销</button>
        <button type="button" className="quiet" disabled={!active_tab || busy} onClick={() => {
          if (!destructive_operation_ref.current && active_document_id) editor_handles_ref.current.get(active_document_id)?.redo();
        }}>重做</button>
        <button
          type="button"
          className="quiet"
          disabled={!active_tab || active_tab.session.saving_revision !== null || !active_tab.session.dirty}
          onClick={() => active_document_id && void save_document(active_document_id)}
        >保存</button>
        <button
          type="button"
          className="quiet theme_toggle"
          data-theme-toggle="true"
          aria-label={theme === "dark" ? "切换到浅色主题" : "切换到深色主题"}
          title={theme === "dark" ? "切换到 VS Code Light+" : "切换到 VS Code Dark+"}
          onClick={() => set_theme((current) => current === "dark" ? "light" : "dark")}
        >{theme === "dark" ? "浅色主题" : "深色主题"}</button>
      </nav>

      <section className="workspace_grid">
        <aside className="explorer" aria-label="资源管理器">
          <h2>资源管理器</h2>
          {!workspace && <p className="muted">尚未打开文件或文件夹</p>}
          {workspace?.mode === "single_file" && (
            <ul className="file_tree" role="tree" aria-label="单文件">
              <li
                role="treeitem"
                tabIndex={0}
                className="single_file_entry"
                onClick={() => set_active_document_id(workspace.document.document_id)}
                onKeyDown={(event) => {
                  if (event.key === "Enter" || event.key === " ") set_active_document_id(workspace.document.document_id);
                }}
              >
                <div className="tree_entry_row"><span className="entry_icon kind_markdown" aria-hidden="true">M</span><span>{workspace.document.name}</span></div>
              </li>
            </ul>
          )}
          {workspace?.mode === "folder" && (
            <ExplorerTree key={workspace.workspace_id} folder={workspace} on_open_markdown={open_folder_entry} on_error={set_message} />
          )}
        </aside>

        <section className="editor_surface" aria-label="文档工作区">
          {tabs.length > 0 && (
            <div className="tab_strip" role="tablist" aria-label="打开的文档">
              {tabs.map((tab) => (
                <div className="tab_item" key={tab.snapshot.document_id}>
                  <button
                    type="button"
                    role="tab"
                    aria-selected={tab.snapshot.document_id === active_document_id}
                    className={tab.snapshot.document_id === active_document_id ? "is_active" : ""}
                    onClick={() => set_active_document_id(tab.snapshot.document_id)}
                  >
                    <span>{tab.snapshot.name}</span><span className="dirty_mark" aria-label={tab.session.dirty ? "未保存" : "已保存"}>{tab.session.dirty ? "●" : ""}</span>
                  </button>
                  {workspace?.mode === "folder" && (
                    <button
                      type="button"
                      className="tab_close"
                      aria-label={`关闭 ${tab.snapshot.name}`}
                      disabled={closing_document_id === tab.snapshot.document_id}
                      onClick={() => void close_document_tab(tab.snapshot.document_id)}
                    >×</button>
                  )}
                </div>
              ))}
            </div>
          )}
          <div className="editor_stack" data-source-mode={source_mode ? "source" : "hybrid"}>
            {!workspace && (
              <div className="welcome">
                <p className="eyebrow">SAFE HYBRID MARKDOWN EDITING</p>
                <h1>打开一个 Markdown 或文件夹</h1>
                <p className="summary">路径只由系统对话框交给本地 C++ 服务。正文通过有界二进制附件进入内存编辑器，Renderer 不接收绝对路径。</p>
                <div className="welcome_actions">
                  <button type="button" onClick={() => void open_file()} disabled={busy}>打开 Markdown</button>
                  <button type="button" onClick={() => void open_folder()} disabled={busy}>打开文件夹</button>
                </div>
              </div>
            )}
            {workspace?.mode === "folder" && tabs.length === 0 && (
              <div className="folder_info">
                <p className="eyebrow">ON-DEMAND WORKSPACE</p>
                <h1>{workspace.display_name}</h1>
                <p className="summary">逐级展开目录并选择 Markdown。非 Markdown、不可访问链接和工作区边界外对象不会获得正文能力。</p>
              </div>
            )}
            {tabs.length > 0 && (
              <div className="source_pane">
                {tabs.map((tab) => (
                  <DocumentEditor
                    key={tab.snapshot.document_id}
                    snapshot={tab.snapshot}
                    active={tab.snapshot.document_id === active_document_id}
                    editable={!busy}
                    preview_document={preview_documents[tab.snapshot.document_id] ?? null}
                    source_mode={source_mode}
                    theme={theme}
                    on_document_change={update_document_state}
                    on_preview_source={update_preview_source}
                    on_position_change={update_position}
                    on_save_requested={(document_id) => void save_document(document_id)}
                    on_source_mode_requested={toggle_source_mode}
                    on_preview_error={set_message}
                    on_editor_handle_change={(document_id, handle) => {
                      if (handle) {
                        editor_handles_ref.current.set(document_id, handle);
                        if (destructive_operation_ref.current) handle.set_editable(false);
                      } else {
                        editor_handles_ref.current.delete(document_id);
                      }
                    }}
                  />
                ))}
              </div>
            )}
          </div>
        </section>
      </section>

      <footer className="statusbar">
        <span>{active_tab ? active_tab.snapshot.display_path : runtime?.native_service.message ?? "正在连接本地服务"}</span>
        <span>
          {active_tab && active_position
            ? `行 ${active_position.line}，列 ${active_position.column} · ${active_tab.snapshot.bom ? "UTF-8 BOM" : "UTF-8"} · ${active_tab.snapshot.line_ending.toUpperCase()} · ${active_tab.session.saving_revision !== null ? "正在保存" : active_tab.session.dirty ? "未保存（仅内存）" : "已保存"} · ${source_mode ? "源码模式" : "混合编辑"} · ${preview_failures[active_tab.snapshot.document_id] ? "渲染异常" : `${preview_documents[active_tab.snapshot.document_id]?.diagnostics.length ?? 0} 个渲染诊断`}`
            : workspace ? (workspace.mode === "folder" ? "文件夹模式" : "正在打开正文") : "尚未选择文件或文件夹"}
        </span>
      </footer>
      {format_decision_document_id && (
        <section className="format_decision" role="alertdialog" aria-label="选择保存换行格式">
          <span>检测到混合换行，保存前必须统一格式。</span>
          <button type="button" onClick={() => void save_document(format_decision_document_id, "normalize_lf")}>统一 LF</button>
          <button type="button" onClick={() => void save_document(format_decision_document_id, "normalize_crlf")}>统一 CRLF</button>
          <button type="button" className="quiet" onClick={() => set_format_decision_document_id(null)}>取消</button>
        </section>
      )}
      {close_decision_document_id && !format_decision_document_id && (
        <section className="close_decision" role="alertdialog" aria-label="处理未保存修改">
          <span>该标签有未保存修改。请选择保存、明确放弃或取消关闭。</span>
          <button type="button" onClick={() => {
            close_after_save_document_id_ref.current = close_decision_document_id;
            void save_document(close_decision_document_id);
          }}>保存并关闭</button>
          <button type="button" onClick={() => {
            close_after_save_document_id_ref.current = null;
            void close_document_tab(close_decision_document_id, true);
          }}>放弃修改</button>
          <button type="button" className="quiet" onClick={() => {
            close_after_save_document_id_ref.current = null;
            set_close_decision_document_id(null);
          }}>取消</button>
        </section>
      )}
      <div className="live_status" role="status" aria-live="polite">{message}</div>
    </main>
  );
}
