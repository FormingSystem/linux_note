import type { TrainingCategory, UserTrainingModule, WorkspaceData } from "../../shared/types";

export function createId(prefix: string) {
  return `${prefix}-${crypto.randomUUID()}`;
}

export function categoryHasDescendant(categories: TrainingCategory[], categoryId: string, candidateId: string): boolean {
  let current = categories.find((item) => item.id === candidateId);
  while (current?.parentId) {
    if (current.parentId === categoryId) return true;
    current = categories.find((item) => item.id === current?.parentId);
  }
  return false;
}

export function mergeCategories(data: WorkspaceData, sourceId: string, targetId: string): WorkspaceData {
  if (sourceId === targetId || categoryHasDescendant(data.categories, sourceId, targetId)) {
    throw new Error("不能把分类合并到自身或其后代");
  }
  return {
    ...data,
    categories: data.categories
      .filter((item) => item.id !== sourceId)
      .map((item) => item.parentId === sourceId ? { ...item, parentId: targetId } : item),
    modules: data.modules.map((item) => ({
      ...item,
      categoryIds: [...new Set(item.categoryIds.map((id) => id === sourceId ? targetId : id))],
    })),
  };
}

export function mergeModules(data: WorkspaceData, sourceId: string, targetId: string): WorkspaceData {
  const source = data.modules.find((item) => item.id === sourceId);
  const target = data.modules.find((item) => item.id === targetId);
  if (!source || !target || sourceId === targetId) throw new Error("请选择两个不同的有效训练模块");
  const now = new Date().toISOString();
  const merged: UserTrainingModule = {
    ...target,
    name: `${target.name}（合并）`,
    description: [target.description, source.description].filter(Boolean).join("\n"),
    unitIds: [...new Set([...target.unitIds, ...source.unitIds])],
    categoryIds: [...new Set([...target.categoryIds, ...source.categoryIds])],
    updatedAt: now,
  };
  return { ...data, modules: data.modules.filter((item) => item.id !== sourceId).map((item) => item.id === targetId ? merged : item) };
}

export function exportWorkspace(data: WorkspaceData) {
  return JSON.stringify({
    format: "loop-training-workspace",
    schemaVersion: 1,
    exportedAt: new Date().toISOString(),
    data,
  }, null, 2);
}

export function importWorkspace(raw: string): WorkspaceData {
  const parsed = JSON.parse(raw) as { format?: string; schemaVersion?: number; data?: WorkspaceData };
  if (parsed.format !== "loop-training-workspace" || parsed.schemaVersion !== 1 || !parsed.data) {
    throw new Error("不是受支持的回路训练工作区文件");
  }
  if (!Array.isArray(parsed.data.categories) || !Array.isArray(parsed.data.modules)) {
    throw new Error("工作区数据结构不完整");
  }
  return parsed.data;
}
