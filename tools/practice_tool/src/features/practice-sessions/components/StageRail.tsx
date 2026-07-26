import { Link } from "react-router-dom";
import type { PracticeSession, SaveState, TrainingStage } from "../../../shared/types";
import { stageItems } from "../../../infrastructure/persistence/sessionRepository";
import { STAGES } from "../model/stages";

export default function StageRail({ session, activeStage, completed, saveState, onSelect }: {
  session: PracticeSession;
  activeStage: TrainingStage;
  completed: Set<string>;
  saveState: SaveState;
  onSelect: (stage: TrainingStage) => void;
}) {
  return (
    <aside className="rail" aria-label="训练阶段">
      <Link className="back-lobby" to="/">← 返回大厅</Link>
      <div className="rail-title">{session.unitTitle}</div>
      <p className="rail-hint">阶段可自由切换，完成标记只表示进度。</p>
      {STAGES.map((entry, index) => {
        const items = stageItems(session, entry.id);
        const done = entry.id === "summary" ? session.status === "completed" : items.length > 0 && items.every((item) => completed.has(item.id));
        return (
          <button key={entry.id} className={`rail-step ${entry.id === activeStage ? "active" : ""} ${done ? "complete" : ""}`} onClick={() => onSelect(entry.id)}>
            <span>{done ? "✓" : index}</span>{entry.label}
          </button>
        );
      })}
      <div className={`save-indicator ${saveState}`} role="status">{saveLabel(saveState)}</div>
    </aside>
  );
}

function saveLabel(state: SaveState) {
  return state === "dirty" ? "有改动待保存" : state === "saving" ? "正在保存…" : state === "failed" ? "保存失败" : state === "saved" ? "已自动保存" : "进度已保存";
}
