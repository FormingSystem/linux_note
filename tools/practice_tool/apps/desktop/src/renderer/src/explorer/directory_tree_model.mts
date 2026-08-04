import type { entry_page, file_entry } from "@loop/ipc-contracts";

export interface directory_tree_state {
  expanded: boolean;
  generation: number;
  loading: boolean;
  error: string | null;
  entries: file_entry[] | null;
  next_cursor: string | null;
  total_entries: number;
}

export function create_directory_tree_state(expanded = false): directory_tree_state {
  return {
    expanded,
    generation: 0,
    loading: false,
    error: null,
    entries: null,
    next_cursor: null,
    total_entries: 0,
  };
}

export function set_directory_expanded(
  state: directory_tree_state,
  expanded: boolean,
): directory_tree_state {
  return { ...state, expanded };
}

export function begin_directory_load(
  state: directory_tree_state,
  generation: number,
): directory_tree_state {
  return { ...state, generation, loading: true, error: null };
}

export function complete_directory_load(
  state: directory_tree_state,
  generation: number,
  page: entry_page,
  append: boolean,
): directory_tree_state {
  if (generation !== state.generation) return state;
  return {
    ...state,
    loading: false,
    error: null,
    entries: append && state.entries ? [...state.entries, ...page.entries] : [...page.entries],
    next_cursor: page.next_cursor,
    total_entries: page.total_entries,
  };
}

export function fail_directory_load(
  state: directory_tree_state,
  generation: number,
  error: string,
): directory_tree_state {
  return generation === state.generation ? { ...state, loading: false, error } : state;
}
