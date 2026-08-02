import { useEffect, useState } from "react";
import type {
  command_result,
  entry_page,
  file_entry,
  opened_folder,
  opened_single_file,
  runtime_info,
} from "@loop/ipc-contracts";

type opened_workspace = opened_single_file | opened_folder;

function format_bytes(value: number | null): string {
  if (value === null) return "—";
  if (value < 1024) return `${value} B`;
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KiB`;
  return `${(value / (1024 * 1024)).toFixed(2)} MiB`;
}

function result_error<value_type>(result: command_result<value_type>): string | null {
  return result.status === "error" ? result.error.user_message : null;
}

export function App() {
  const [runtime, set_runtime] = useState<runtime_info | null>(null);
  const [workspace, set_workspace] = useState<opened_workspace | null>(null);
  const [entries, set_entries] = useState<file_entry[]>([]);
  const [next_cursor, set_next_cursor] = useState<string | null>(null);
  const [total_entries, set_total_entries] = useState(0);
  const [busy, set_busy] = useState(false);
  const [message, set_message] = useState<string | null>(null);

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

  async function load_folder_page(folder: opened_folder, cursor?: string): Promise<void> {
    const result = await window.loop.explorer.list_children({
      workspace_id: folder.workspace_id,
      directory_id: folder.root_directory_id,
      ...(cursor ? { cursor } : {}),
    });
    if (result.status === "error") {
      set_message(result.error.user_message);
      return;
    }
    if (result.status !== "ok") return;
    const page: entry_page = result.value;
    set_entries((current) => cursor ? [...current, ...page.entries] : page.entries);
    set_next_cursor(page.next_cursor);
    set_total_entries(page.total_entries);
  }

  async function open_file(): Promise<void> {
    set_busy(true);
    set_message(null);
    try {
      const result = await window.loop.workbench.open_file();
      const error = result_error(result);
      if (error) set_message(error);
      if (result.status === "ok") {
        set_workspace(result.value);
        set_entries([]);
        set_next_cursor(null);
        set_total_entries(0);
      }
    } catch {
      set_message("打开文件请求失败");
    } finally {
      set_busy(false);
    }
  }

  async function open_folder(): Promise<void> {
    set_busy(true);
    set_message(null);
    try {
      const result = await window.loop.workbench.open_folder();
      const error = result_error(result);
      if (error) set_message(error);
      if (result.status === "ok") {
        set_workspace(result.value);
        set_entries([]);
        set_next_cursor(null);
        set_total_entries(0);
        await load_folder_page(result.value);
      }
    } catch {
      set_message("打开文件夹请求失败");
    } finally {
      set_busy(false);
    }
  }

  async function load_more(): Promise<void> {
    if (!workspace || workspace.mode !== "folder" || !next_cursor) return;
    set_busy(true);
    set_message(null);
    try {
      await load_folder_page(workspace, next_cursor);
    } catch {
      set_message("继续读取目录失败");
    } finally {
      set_busy(false);
    }
  }

  async function close_workspace(): Promise<void> {
    set_busy(true);
    set_message(null);
    try {
      const result = await window.loop.workbench.close_workspace();
      const error = result_error(result);
      if (error) set_message(error);
      if (result.status === "ok") {
        set_workspace(null);
        set_entries([]);
        set_next_cursor(null);
        set_total_entries(0);
      }
    } catch {
      set_message("关闭工作区请求失败");
    } finally {
      set_busy(false);
    }
  }

  return (
    <main className="workbench">
      <header className="titlebar">
        <span className="product_mark" aria-hidden="true">◉</span>
        <span className="window_title">回路（Loop）Markdown 工作台</span>
        <span className="workspace_title">{workspace?.display_name ?? "空窗口"}</span>
      </header>

      <nav className="commandbar" aria-label="工作台命令">
        <button type="button" onClick={() => void open_file()} disabled={busy}>打开文件</button>
        <button type="button" onClick={() => void open_folder()} disabled={busy}>打开文件夹</button>
        {workspace && (
          <button type="button" className="quiet" onClick={() => void close_workspace()} disabled={busy}>
            关闭工作区
          </button>
        )}
      </nav>

      <section className="workspace_grid">
        <aside className="explorer" aria-label="资源管理器">
          <h2>资源管理器</h2>
          {!workspace && <p className="muted">尚未打开文件或文件夹</p>}
          {workspace?.mode === "single_file" && (
            <ul className="file_tree" role="tree" aria-label="单文件">
              <li role="treeitem"><span className="entry_icon" aria-hidden="true">M</span>{workspace.document.name}</li>
            </ul>
          )}
          {workspace?.mode === "folder" && (
            <>
              <p className="root_label">{workspace.display_name}</p>
              <ul className="file_tree" role="tree" aria-label={`${workspace.display_name} 文件列表`}>
                {entries.map((entry) => (
                  <li
                    key={entry.entry_id}
                    role="treeitem"
                    aria-disabled={!entry.accessible}
                    title={entry.accessible ? entry.relative_path : "链接、挂载边界或无效名称不可访问"}
                  >
                    <span className={`entry_icon kind_${entry.kind}`} aria-hidden="true">
                      {entry.kind === "directory" ? "D" : entry.kind === "markdown" ? "M" : "·"}
                    </span>
                    <span>{entry.name}</span>
                  </li>
                ))}
              </ul>
              {next_cursor && (
                <button type="button" className="load_more" onClick={() => void load_more()} disabled={busy}>
                  继续加载（{entries.length}/{total_entries}）
                </button>
              )}
            </>
          )}
        </aside>

        <section className="editor_surface" aria-labelledby="surface-title">
          {!workspace && (
            <div className="welcome">
              <p className="eyebrow">D1A FILE CAPABILITIES</p>
              <h1 id="surface-title">打开一个 Markdown 或文件夹</h1>
              <p className="summary">
                路径只由系统对话框交给本地 C++ 服务。工作台只接收显示标签、相对路径和不透明 ID。
              </p>
              <div className="welcome_actions">
                <button type="button" onClick={() => void open_file()} disabled={busy}>打开 Markdown</button>
                <button type="button" onClick={() => void open_folder()} disabled={busy}>打开文件夹</button>
              </div>
            </div>
          )}

          {workspace?.mode === "single_file" && (
            <article className="document_info">
              <p className="eyebrow">READ-ONLY DOCUMENT CAPABILITY</p>
              <h1 id="surface-title">{workspace.document.name}</h1>
              <p className="summary">文件已经完成安全读取和 UTF-8 校验。编辑、预览与保存将在后续切片开放。</p>
              <dl className="metadata_card">
                <div><dt>显示路径</dt><dd>{workspace.document.display_path}</dd></div>
                <div><dt>大小</dt><dd>{format_bytes(workspace.document.byte_size)}</dd></div>
                <div><dt>编码</dt><dd>{workspace.document.bom ? "UTF-8 BOM" : "UTF-8"}</dd></div>
                <div><dt>换行</dt><dd>{workspace.document.line_ending.toUpperCase()}</dd></div>
                <div><dt>写入状态</dt><dd>{workspace.document.read_only ? "只读" : "尚未开放保存"}</dd></div>
                <div><dt>链接解析</dt><dd>{workspace.document.resolved_from_link ? "通过显式选择解析" : "普通文件"}</dd></div>
              </dl>
            </article>
          )}

          {workspace?.mode === "folder" && (
            <article className="folder_info">
              <p className="eyebrow">FOLDER CAPABILITY</p>
              <h1 id="surface-title">{workspace.display_name}</h1>
              <p className="summary">
                已读取首层目录元数据，共 {total_entries} 项；没有读取任何文件正文。子目录展开和文件编辑尚未开放。
              </p>
            </article>
          )}
        </section>
      </section>

      <footer className="statusbar">
        <span>{busy ? "正在处理…" : runtime?.native_service.message ?? "正在连接本地服务"}</span>
        <span>{workspace ? (workspace.mode === "folder" ? "文件夹模式" : "单文件模式") : "无磁盘能力"}</span>
      </footer>
      <div className="live_status" role="status" aria-live="polite">{message}</div>
    </main>
  );
}
