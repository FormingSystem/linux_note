import type { WorkspaceData } from "../../shared/types";
import { readRecord, STORES, writeRecord } from "./database";

const RECORD_KEY = "current";
const LEGACY_STORAGE_KEY = "loop-knowledge-practice.workspace.v1";
const EMPTY_WORKSPACE: WorkspaceData = { schemaVersion: 1, categories: [], modules: [], unitAssignmentMode: "exclusive", historyLimit: 1000 };

export async function loadWorkspace(): Promise<WorkspaceData> {
  const stored = await readRecord<WorkspaceData>(STORES.workspace, RECORD_KEY);
  if (stored) return normalizeWorkspace(stored);

  try {
    const legacy = localStorage.getItem(LEGACY_STORAGE_KEY);
    if (!legacy) return EMPTY_WORKSPACE;
    const migrated = JSON.parse(legacy) as WorkspaceData;
    await saveWorkspace(migrated);
    localStorage.removeItem(LEGACY_STORAGE_KEY);
    return normalizeWorkspace(migrated);
  } catch {
    return EMPTY_WORKSPACE;
  }
}

function normalizeWorkspace(data: WorkspaceData): WorkspaceData {
  const historyLimit = Number.isFinite(data.historyLimit) ? Math.min(10000, Math.max(1, Math.round(data.historyLimit!))) : 1000;
  return { ...data, unitAssignmentMode: data.unitAssignmentMode ?? "exclusive", historyLimit, categories: data.categories.map((item, index) => ({ ...item, unitIds: item.unitIds ?? [], sortOrder: item.sortOrder ?? index })) };
}

export function saveWorkspace(data: WorkspaceData): Promise<void> {
  return writeRecord(STORES.workspace, normalizeWorkspace(data), RECORD_KEY);
}
