import assert from "node:assert/strict";
import test from "node:test";
import type { entry_page, file_entry } from "@loop/ipc-contracts";
import {
  begin_directory_load,
  complete_directory_load,
  create_directory_tree_state,
  set_directory_expanded,
} from "../src/renderer/src/explorer/directory_tree_model.mts";
import { add_document_tab } from "../src/renderer/src/editor/document_tab_model.mts";

function entry(entry_id: string, name: string): file_entry {
  return {
    entry_id,
    parent_id: "directory_0123456789abcdef0123456789abcdef",
    name,
    relative_path: name,
    kind: "markdown",
    expandable: false,
    accessible: true,
    byte_size: 1,
  };
}

function page(entries: file_entry[], next_cursor: string | null, total_entries: number): entry_page {
  return {
    workspace_id: "workspace_0123456789abcdef0123456789abcdef",
    directory_id: "directory_0123456789abcdef0123456789abcdef",
    entries,
    next_cursor,
    total_entries,
  };
}

test("目录折叠保留缓存且重新展开不丢分页状态", () => {
  const first_entry = entry("entry_11111111111111111111111111111111", "a.md");
  let state = begin_directory_load(create_directory_tree_state(true), 1);
  state = complete_directory_load(state, 1, page([first_entry], "cursor_11111111111111111111111111111111", 2), false);
  state = set_directory_expanded(state, false);
  state = set_directory_expanded(state, true);
  assert.deepEqual(state.entries, [first_entry]);
  assert.equal(state.next_cursor, "cursor_11111111111111111111111111111111");
});

test("目录分页追加，刷新替换子树，过期响应被丢弃", () => {
  const first_entry = entry("entry_11111111111111111111111111111111", "a.md");
  const second_entry = entry("entry_22222222222222222222222222222222", "b.md");
  const refreshed_entry = entry("entry_33333333333333333333333333333333", "c.md");
  let state = begin_directory_load(create_directory_tree_state(true), 1);
  state = complete_directory_load(state, 1, page([first_entry], "cursor_11111111111111111111111111111111", 2), false);
  state = begin_directory_load(state, 2);
  state = complete_directory_load(state, 2, page([second_entry], null, 2), true);
  assert.deepEqual(state.entries, [first_entry, second_entry]);
  state = begin_directory_load(state, 3);
  const stale_state = complete_directory_load(state, 2, page([first_entry], null, 1), false);
  assert.equal(stale_state, state);
  state = complete_directory_load(state, 3, page([refreshed_entry], null, 1), false);
  assert.deepEqual(state.entries, [refreshed_entry]);
});

test("标签按 document_id 去重并执行窗口上限", () => {
  const first = { snapshot: { document_id: "document_a" }, value: 1 };
  const duplicate = { snapshot: { document_id: "document_a" }, value: 2 };
  const second = { snapshot: { document_id: "document_b" }, value: 3 };
  const added = add_document_tab([], first, 1);
  assert.equal(added.status, "added");
  const existing = add_document_tab(added.tabs, duplicate, 1);
  assert.equal(existing.status, "existing");
  assert.equal(existing.tabs[0]?.value, 1);
  assert.equal(add_document_tab(existing.tabs, second, 1).status, "limit");
});
