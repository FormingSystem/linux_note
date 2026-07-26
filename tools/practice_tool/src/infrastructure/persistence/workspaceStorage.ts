import type { WorkspaceData } from "../../shared/types";

const STORAGE_KEY = "loop-knowledge-practice.workspace.v1";

export function loadWorkspace(): WorkspaceData {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return raw ? JSON.parse(raw) as WorkspaceData : { schemaVersion: 1, categories: [], modules: [] };
  } catch {
    return { schemaVersion: 1, categories: [], modules: [] };
  }
}

export function saveWorkspace(data: WorkspaceData) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
}
