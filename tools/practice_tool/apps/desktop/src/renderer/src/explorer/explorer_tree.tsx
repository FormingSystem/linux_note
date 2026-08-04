import { createContext, useContext, useEffect, useRef, useState, type KeyboardEvent, type RefObject } from "react";
import type { file_entry, opened_folder } from "@loop/ipc-contracts";
import {
  begin_directory_load,
  complete_directory_load,
  create_directory_tree_state,
  fail_directory_load,
  set_directory_expanded,
} from "./directory_tree_model.mts";

interface tree_focus_state {
  focused_id: string | null;
  set_focused_id(value: string): void;
  tree_ref: RefObject<HTMLUListElement | null>;
}

const tree_focus_context = createContext<tree_focus_state | null>(null);

interface explorer_tree_properties {
  folder: opened_folder;
  on_open_markdown(entry: file_entry): void;
  on_error(message: string): void;
}

interface directory_children_properties extends explorer_tree_properties {
  directory_id: string;
  visible: boolean;
  group: boolean;
}

function visible_tree_items(tree: HTMLUListElement): HTMLElement[] {
  return [...tree.querySelectorAll<HTMLElement>('[role="treeitem"]')]
    .filter((item) => item.getClientRects().length > 0);
}

function move_focus(
  event: KeyboardEvent<HTMLElement>,
  context: tree_focus_state | null,
  destination: "next" | "previous" | "first" | "last",
): void {
  if (!context?.tree_ref.current) return;
  const items = visible_tree_items(context.tree_ref.current);
  const current_index = items.indexOf(event.currentTarget);
  const index = destination === "first" ? 0
    : destination === "last" ? items.length - 1
      : destination === "next" ? Math.min(items.length - 1, current_index + 1)
        : Math.max(0, current_index - 1);
  items[index]?.focus();
}

function DirectoryChildren(properties: directory_children_properties) {
  const [state, set_state] = useState(() => create_directory_tree_state(properties.visible));
  const generation_ref = useRef(0);

  async function load(reset: boolean): Promise<void> {
    if (state.loading) return;
    const generation = ++generation_ref.current;
    const cursor = reset ? undefined : state.next_cursor ?? undefined;
    set_state((current) => begin_directory_load(current, generation));
    try {
      const result = await window.loop.explorer.list_children({
        workspace_id: properties.folder.workspace_id,
        directory_id: properties.directory_id,
        ...(cursor ? { cursor } : {}),
      });
      if (generation !== generation_ref.current) return;
      if (result.status === "error") {
        set_state((current) => fail_directory_load(current, generation, result.error.user_message));
        properties.on_error(result.error.user_message);
        return;
      }
      if (result.status !== "ok") return;
      set_state((current) => complete_directory_load(current, generation, result.value, !reset));
    } catch {
      if (generation !== generation_ref.current) return;
      set_state((current) => fail_directory_load(current, generation, "目录读取失败"));
      properties.on_error("目录读取失败");
    } finally {
      if (generation === generation_ref.current) {
        set_state((current) => current.generation === generation ? { ...current, loading: false } : current);
      }
    }
  }

  useEffect(() => {
    set_state((current) => set_directory_expanded(current, properties.visible));
    if (properties.visible && state.entries === null && !state.loading) void load(true);
  }, [properties.visible]);

  useEffect(() => () => {
    ++generation_ref.current;
  }, [properties.folder.workspace_id, properties.directory_id]);

  const content = (
    <>
      {state.entries?.map((entry) => (
        <TreeEntry key={entry.entry_id} entry={entry} {...properties} />
      ))}
      {state.error && <li className="tree_message" role="none">{state.error}</li>}
      {(state.next_cursor || state.loading) && (
        <li className="tree_controls" role="none">
          <button type="button" className="tree_action" disabled={state.loading || !state.next_cursor} onClick={(event) => { event.stopPropagation(); void load(false); }}>
            {state.loading ? "加载中…" : `继续加载（${state.entries?.length ?? 0}/${state.total_entries}）`}
          </button>
        </li>
      )}
      {state.entries && (
        <li className="tree_controls" role="none">
          <button type="button" className="tree_action" disabled={state.loading} onClick={(event) => { event.stopPropagation(); void load(true); }}>刷新此目录</button>
        </li>
      )}
    </>
  );

  return properties.group
    ? <ul className="file_tree nested_tree" role="group" hidden={!properties.visible}>{content}</ul>
    : <>{content}</>;
}

interface tree_entry_properties extends directory_children_properties {
  entry: file_entry;
}

function TreeEntry(properties: tree_entry_properties) {
  const { entry } = properties;
  const focus = useContext(tree_focus_context);
  const [expanded, set_expanded] = useState(false);
  const [activated, set_activated] = useState(false);
  const is_directory = entry.kind === "directory" && entry.accessible;

  useEffect(() => {
    if (!focus) return;
    const focused_item = focus.focused_id
      ? focus.tree_ref.current?.querySelector<HTMLElement>(`[data-entry-id="${focus.focused_id}"]`) ?? null
      : null;
    if (!focused_item || focused_item.getClientRects().length === 0) {
      const first_visible = focus.tree_ref.current ? visible_tree_items(focus.tree_ref.current)[0] : null;
      if (first_visible?.dataset.entryId === entry.entry_id) focus.set_focused_id(entry.entry_id);
    }
  }, [entry.entry_id, focus]);

  function activate(): void {
    if (!entry.accessible) return;
    if (is_directory) {
      focus?.set_focused_id(entry.entry_id);
      set_activated(true);
      set_expanded((value) => !value);
    } else if (entry.kind === "markdown") {
      properties.on_open_markdown(entry);
    }
  }

  function on_key_down(event: KeyboardEvent<HTMLLIElement>): void {
    event.stopPropagation();
    if (event.key === "ArrowDown" || event.key === "ArrowUp" || event.key === "Home" || event.key === "End") {
      event.preventDefault();
      move_focus(event, focus, event.key === "ArrowDown" ? "next" : event.key === "ArrowUp" ? "previous" : event.key === "Home" ? "first" : "last");
      return;
    }
    if (event.key === "ArrowRight" && is_directory) {
      event.preventDefault();
      if (!expanded) {
        focus?.set_focused_id(entry.entry_id);
        set_activated(true);
        set_expanded(true);
      } else {
        event.currentTarget.querySelector<HTMLElement>('[role="group"] [role="treeitem"]')?.focus();
      }
      return;
    }
    if (event.key === "ArrowLeft") {
      event.preventDefault();
      if (is_directory && expanded) {
        focus?.set_focused_id(entry.entry_id);
        set_expanded(false);
      }
      else event.currentTarget.parentElement?.closest<HTMLElement>('[role="treeitem"]')?.focus();
      return;
    }
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      activate();
    }
  }

  return (
    <li
      role="treeitem"
      data-entry-id={entry.entry_id}
      tabIndex={focus?.focused_id === entry.entry_id ? 0 : -1}
      aria-expanded={is_directory ? expanded : undefined}
      aria-disabled={!entry.accessible}
      title={entry.accessible ? entry.relative_path : "链接、挂载边界或无效名称不可访问"}
      onFocus={() => focus?.set_focused_id(entry.entry_id)}
      onClick={(event) => {
        event.stopPropagation();
        if (event.target === event.currentTarget || (event.target as HTMLElement).closest(".tree_entry_row")) activate();
      }}
      onKeyDown={on_key_down}
    >
      <div className="tree_entry_row">
        <span className={`entry_icon kind_${entry.kind}`} aria-hidden="true">
          {is_directory ? (expanded ? "▾" : "▸") : entry.kind === "markdown" ? "M" : "·"}
        </span>
        <span>{entry.name}</span>
      </div>
      {is_directory && activated && (
        <DirectoryChildren {...properties} directory_id={entry.entry_id} visible={expanded} group />
      )}
    </li>
  );
}

export function ExplorerTree(properties: explorer_tree_properties) {
  const [focused_id, set_focused_id] = useState<string | null>(null);
  const tree_ref = useRef<HTMLUListElement>(null);
  return (
    <tree_focus_context.Provider value={{ focused_id, set_focused_id, tree_ref }}>
      <p className="root_label">{properties.folder.display_name}</p>
      <ul ref={tree_ref} className="file_tree" role="tree" aria-label={`${properties.folder.display_name} 文件树`}>
        <DirectoryChildren {...properties} directory_id={properties.folder.root_directory_id} visible group={false} />
      </ul>
    </tree_focus_context.Provider>
  );
}
