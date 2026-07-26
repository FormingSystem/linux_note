import { useEffect, useState } from "react";
import { loadWorkspace, saveWorkspace } from "../../infrastructure/persistence/workspaceRepository";
import type { TrainingCategory, WorkspaceData } from "../../shared/types";

type AssignmentMode = NonNullable<WorkspaceData["unitAssignmentMode"]>;

export default function SettingsPage() {
  const [workspace, setWorkspace] = useState<WorkspaceData>();
  const [savedMessage, setSavedMessage] = useState("");
  const [historyLimitInput, setHistoryLimitInput] = useState("1000");

  useEffect(() => {
    void loadWorkspace().then((stored) => {
      setWorkspace(stored);
      setHistoryLimitInput(String(stored.historyLimit ?? 1000));
    });
  }, []);

  const selectMode = async (mode: AssignmentMode) => {
    if (!workspace || workspace.unitAssignmentMode === mode) return;
    const next = {
      ...workspace,
      unitAssignmentMode: mode,
      categories: mode === "exclusive" ? enforceExclusiveAssignments(workspace.categories) : workspace.categories,
    };
    await saveWorkspace(next);
    setWorkspace(next);
    setSavedMessage(mode === "exclusive" ? "已改为单一目录，重复归属已按目录顺序整理。" : "已允许一个单元加入多个目录。");
  };
  const saveHistoryLimit = async () => {
    if (!workspace) return;
    const parsed = Number(historyLimitInput);
    const historyLimit = Number.isFinite(parsed) ? Math.min(10000, Math.max(1, Math.round(parsed))) : 1000;
    const next = { ...workspace, historyLimit };
    await saveWorkspace(next);
    setWorkspace(next);
    setHistoryLimitInput(String(historyLimit));
    setSavedMessage(`撤销历史已设置为 ${historyLimit} 步。`);
  };

  return (
    <section className="settings-page">
      <header>
        <span className="eyebrow">PREFERENCES</span>
        <h1>设置</h1>
        <p>调整训练库的组织方式。设置只保存在本机，并立即生效。</p>
      </header>
      <section className="setting-section" aria-labelledby="assignment-mode-heading">
        <div className="setting-section-heading">
          <div>
            <h2 id="assignment-mode-heading">单元归类方式</h2>
            <p>决定同一个训练单元能否同时出现在多个目录中。</p>
          </div>
          {savedMessage && <span className="setting-saved" role="status">{savedMessage}</span>}
        </div>
        <div className="setting-options" role="radiogroup" aria-labelledby="assignment-mode-heading">
          <button type="button" role="radio" aria-checked={(workspace?.unitAssignmentMode ?? "exclusive") === "exclusive"} className={(workspace?.unitAssignmentMode ?? "exclusive") === "exclusive" ? "selected" : ""} onClick={() => void selectMode("exclusive")}>
            <span><strong>单一目录</strong><small>默认</small></span>
            <p>一个单元只属于一个目录。加入新目录时，会自动从原目录移出。</p>
          </button>
          <button type="button" role="radio" aria-checked={workspace?.unitAssignmentMode === "multiple"} className={workspace?.unitAssignmentMode === "multiple" ? "selected" : ""} onClick={() => void selectMode("multiple")}>
            <span><strong>多目录</strong></span>
            <p>一个单元可以同时属于多个目录。编辑目录时会标明它已有的全部归属。</p>
          </button>
        </div>
      </section>
      <section className="setting-section" aria-labelledby="history-limit-heading">
        <div className="setting-section-heading">
          <div>
            <h2 id="history-limit-heading">撤销历史步数</h2>
            <p>控制训练库目录整理可以撤销和重做的操作数量，默认保留 1000 步。</p>
          </div>
        </div>
        <div className="history-limit-setting">
          <label htmlFor="history-limit">保留步数</label>
          <input id="history-limit" type="number" min="1" max="10000" step="1" value={historyLimitInput} onChange={(event) => setHistoryLimitInput(event.target.value)} />
          <button type="button" onClick={() => void saveHistoryLimit()}>保存设置</button>
          <small>可设置 1–10000 步；历史仅保留在当前页面会话中。</small>
        </div>
      </section>
    </section>
  );
}

function enforceExclusiveAssignments(categories: TrainingCategory[]): TrainingCategory[] {
  const claimedUnitIds = new Set<string>();
  const normalizedById = new Map(
    [...categories].sort((left, right) => left.sortOrder - right.sortOrder).map((category) => {
      const unitIds = (category.unitIds ?? []).filter((unitId) => {
        if (claimedUnitIds.has(unitId)) return false;
        claimedUnitIds.add(unitId);
        return true;
      });
      return [category.id, { ...category, unitIds }] as const;
    }),
  );
  return categories.map((category) => normalizedById.get(category.id) ?? category);
}
