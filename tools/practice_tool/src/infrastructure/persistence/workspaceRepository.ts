import type { WorkspaceData } from "../../shared/types";
import { readRecord, STORES, writeRecord } from "./database";

const RECORD_KEY = "current";
const LEGACY_STORAGE_KEY = "loop-knowledge-practice.workspace.v1";
const EMPTY_WORKSPACE: WorkspaceData = { schemaVersion: 1, categories: [], modules: [] };

export async function loadWorkspace(): Promise<WorkspaceData> {
  const stored = await readRecord<WorkspaceData>(STORES.workspace, RECORD_KEY);
  if (stored) return stored;

  try {
    const legacy = localStorage.getItem(LEGACY_STORAGE_KEY);
    if (!legacy) return EMPTY_WORKSPACE;
    const migrated = JSON.parse(legacy) as WorkspaceData;
    await saveWorkspace(migrated);
    localStorage.removeItem(LEGACY_STORAGE_KEY);
    return migrated;
  } catch {
    return EMPTY_WORKSPACE;
  }
}

export function saveWorkspace(data: WorkspaceData): Promise<void> {
  return writeRecord(STORES.workspace, data, RECORD_KEY);
}
