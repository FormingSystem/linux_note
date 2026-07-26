import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import type {
  GuidedQuestion, KnowledgeRef, LearningGuide, ModelTask, PracticeSession,
  ProfessionalCase, Rating, SaveState, TrainingStage,
} from "../../shared/types";
import { loadSession, saveSession, stageItems } from "../../infrastructure/persistence/sessionRepository";

const STAGES: Array<{ id: TrainingStage; label: string }> = [
  { id: "learning", label: "学习导引" },
  { id: "guided", label: "提示提问" },
  { id: "reconstruction", label: "脱稿输出" },
  { id: "professional", label: "专业案例" },
  { id: "summary", label: "训练总结" },
];

export default function SessionPage() {
  const { sessionId = "", stage = "", itemId = "" } = useParams();
  const navigate = useNavigate();
  const [session, setSession] = useState<PracticeSession>();
  const [loadError, setLoadError] = useState("");
  const [saveState, setSaveState] = useState<SaveState>("clean");
  const saveTimer = useRef<number | undefined>(undefined);
  const latest = useRef<PracticeSession | undefined>(undefined);

  useEffect(() => {
    void loadSession(sessionId).then((value) => {
      if (!value) return setLoadError("这条训练记录不存在或已被清理。");
      setSession(value);
      latest.current = value;
    }).catch(() => setLoadError("无法读取本地训练记录。"));
  }, [sessionId]);

  const persist = useCallback(async (value: PracticeSession) => {
    window.clearTimeout(saveTimer.current);
    setSaveState("saving");
    try {
      const next = { ...value, updatedAt: new Date().toISOString(), revision: value.revision + 1 };
      await saveSession(next);
      latest.current = next;
      setSession(next);
      setSaveState("saved");
    } catch {
      setSaveState("failed");
    }
  }, []);

  const update = (recipe: (current: PracticeSession) => PracticeSession) => {
    setSession((current) => {
      if (!current) return current;
      const next = recipe(current);
      latest.current = next;
      setSaveState("dirty");
      window.clearTimeout(saveTimer.current);
      saveTimer.current = window.setTimeout(() => void persist(next), 1000);
      return next;
    });
  };

  useEffect(() => () => {
    window.clearTimeout(saveTimer.current);
    if (latest.current) void saveSession({ ...latest.current, updatedAt: new Date().toISOString() });
  }, []);

  const validStage = STAGES.some((entry) => entry.id === stage) ? stage as TrainingStage : undefined;
  const currentItems = useMemo(() => session && validStage ? stageItems(session, validStage) : [], [session, validStage]);
  const index = currentItems.findIndex((entry) => entry.id === itemId);

  useEffect(() => {
    if (!session || !validStage) return;
    if (validStage === "summary") return;
    if (index < 0) {
      const fallback = stageItems(session, validStage)[0]?.id;
      if (fallback) navigate(`/sessions/${session.id}/${validStage}/${fallback}`, { replace: true });
    }
  }, [index, navigate, session, validStage]);

  if (loadError) return <section className="panel error-state"><h1>无法继续训练</h1><p>{loadError}</p><Link to="/">返回大厅</Link></section>;
  if (!session) return <section className="panel loading-state">正在恢复训练进度……</section>;
  if (!validStage) return <RedirectToCurrent session={session} />;

  const completed = new Set(session.completedItemIds);
  const stageUnlocked = (target: TrainingStage) => {
    const targetIndex = STAGES.findIndex((entry) => entry.id === target);
    return STAGES.slice(0, targetIndex).every((entry) => stageItems(session, entry.id).every((item) => completed.has(item.id)));
  };
  if (!stageUnlocked(validStage)) {
    return (
      <>
        <aside className="rail"><Link className="back-lobby" to="/">← 返回大厅</Link></aside>
        <section className="workspace"><div className="panel error-state"><h1>这个阶段尚未解锁</h1><p>请先完成前一阶段的全部训练项。</p><Link to={`/sessions/${session.id}/${session.currentStage}/${session.currentItemId}`}>回到当前进度</Link></div></section>
      </>
    );
  }
  const goStage = (target: TrainingStage) => {
    if (!stageUnlocked(target)) return;
    const targetId = target === "summary" ? "complete" : stageItems(session, target)[0]?.id;
    if (targetId) navigate(`/sessions/${session.id}/${target}/${targetId}`);
  };
  const routeTo = (targetStage: TrainingStage, targetItemId: string) => {
    update((current) => ({ ...current, currentStage: targetStage, currentItemId: targetItemId }));
    navigate(`/sessions/${session.id}/${targetStage}/${targetItemId}`);
    window.scrollTo({ top: 0, behavior: "smooth" });
  };
  const finishItem = () => {
    if (!session.ratings[itemId]) return;
    const nextCompleted = [...new Set([...session.completedItemIds, itemId])];
    const nextItem = currentItems[index + 1];
    if (nextItem) {
      update((current) => ({ ...current, completedItemIds: nextCompleted, currentStage: validStage, currentItemId: nextItem.id }));
      navigate(`/sessions/${session.id}/${validStage}/${nextItem.id}`);
      window.scrollTo({ top: 0, behavior: "smooth" });
      return;
    }
    const nextStage = STAGES[STAGES.findIndex((entry) => entry.id === validStage) + 1]?.id ?? "summary";
    const nextId = nextStage === "summary" ? "complete" : stageItems(session, nextStage)[0]?.id;
    update((current) => ({
      ...current,
      completedItemIds: nextCompleted,
      currentStage: nextStage,
      currentItemId: nextId,
      status: nextStage === "summary" ? "completed" : current.status,
      completedAt: nextStage === "summary" ? new Date().toISOString() : current.completedAt,
    }));
    navigate(`/sessions/${session.id}/${nextStage}/${nextId}`);
  };
  const previous = () => {
    const prior = currentItems[index - 1];
    if (prior) return routeTo(validStage, prior.id);
    const stageIndex = STAGES.findIndex((entry) => entry.id === validStage);
    const priorStage = STAGES[stageIndex - 1]?.id;
    if (!priorStage) return;
    const items = stageItems(session, priorStage);
    const priorId = items.at(-1)?.id;
    if (priorId) routeTo(priorStage, priorId);
  };

  return (
    <>
      <aside className="rail" aria-label="训练阶段">
        <Link className="back-lobby" to="/">← 返回大厅</Link>
        <div className="rail-title">{session.unitTitle}</div>
        {STAGES.map((entry, stageIndex) => {
          const unlocked = stageUnlocked(entry.id);
          const done = stageItems(session, entry.id).every((item) => completed.has(item.id)) && entry.id !== "summary";
          return <button key={entry.id} className={`rail-step ${entry.id === validStage ? "active" : ""} ${done ? "complete" : ""}`} disabled={!unlocked} onClick={() => goStage(entry.id)}><span>{done ? "✓" : stageIndex}</span>{entry.label}</button>;
        })}
        <div className={`save-indicator ${saveState}`} role="status">{saveLabel(saveState)}</div>
      </aside>
      <section className="workspace">
        {validStage === "summary" ? (
          <Summary session={session} />
        ) : (
          <TrainingItem
            session={session}
            stage={validStage}
            itemId={itemId}
            index={index}
            total={currentItems.length}
            onUpdate={update}
            onPrevious={previous}
            onNext={finishItem}
          />
        )}
        {saveState === "failed" && (
          <div className="save-error" role="alert">
            <span>保存失败。当前内容仍保留在页面中。</span>
            <button onClick={() => void persist(session)}>重试保存</button>
            <button onClick={() => exportDraft(session)}>导出草稿</button>
          </div>
        )}
      </section>
    </>
  );
}

function RedirectToCurrent({ session }: { session: PracticeSession }) {
  const navigate = useNavigate();
  useEffect(() => { navigate(`/sessions/${session.id}/${session.currentStage}/${session.currentItemId}`, { replace: true }); }, [navigate, session]);
  return null;
}

type ItemProps = {
  session: PracticeSession; stage: TrainingStage; itemId: string; index: number; total: number;
  onUpdate: (recipe: (current: PracticeSession) => PracticeSession) => void;
  onPrevious: () => void; onNext: () => void;
};

function TrainingItem({ session, stage, itemId, index, total, onUpdate, onPrevious, onNext }: ItemProps) {
  const content = session.contentSnapshot;
  const item = stage === "learning" ? content.learning.find((entry) => entry.id === itemId)
    : stage === "guided" ? content.guided.find((entry) => entry.id === itemId)
    : stage === "reconstruction" ? content.models.find((entry) => entry.id === itemId)
    : content.cases.find((entry) => entry.id === itemId);
  if (!item) return <div className="panel error-state">训练题不存在。</div>;
  const revealed = session.revealedItemIds.includes(itemId);
  const answer = session.answers[itemId] ?? "";
  const rating = session.ratings[itemId];
  const setAnswer = (value: string) => onUpdate((current) => ({ ...current, answers: { ...current.answers, [itemId]: value } }));
  const reveal = () => onUpdate((current) => ({ ...current, revealedItemIds: [...new Set([...current.revealedItemIds, itemId])] }));
  const rate = (value: Rating) => onUpdate((current) => ({ ...current, ratings: { ...current.ratings, [itemId]: value } }));

  return (
    <div className="panel">
      <div className="panel-heading"><div><div className="eyebrow">{STAGES.find((entry) => entry.id === stage)?.label}</div><h2>{item.title}</h2></div><div className="point-count">{index + 1} / {total}</div></div>
      {stage === "learning" && <LearningContent item={item as LearningGuide} refs={content.unit.knowledge_refs} revealed={revealed} />}
      {stage === "guided" && <GuidedContent item={item as GuidedQuestion} hintLevel={session.hintLevels[itemId] ?? 0} revealed={revealed} onHint={() => onUpdate((current) => ({ ...current, hintLevels: { ...current.hintLevels, [itemId]: Math.min((current.hintLevels[itemId] ?? 0) + 1, (item as GuidedQuestion).hints.length) } }))} />}
      {stage === "reconstruction" && <ModelContent item={item as ModelTask} revealed={revealed} />}
      {stage === "professional" && <CaseContent item={item as ProfessionalCase} revealed={revealed} />}
      <textarea className="answer-box" value={answer} onChange={(event) => setAnswer(event.target.value)} placeholder="用自己的话完成回答。内容会自动保存在本机……" />
      <RatingBar value={rating} onRate={rate} />
      <div className="actions">
        <button className="secondary" onClick={onPrevious} disabled={stage === "learning" && index === 0}>← 上一项</button>
        {!revealed ? <button className="secondary" onClick={reveal}>查看答案与核验点</button> : <button className="primary" disabled={!rating} onClick={onNext}>完成并继续 →</button>}
      </div>
    </div>
  );
}

function LearningContent({ item, refs, revealed }: { item: LearningGuide; refs: KnowledgeRef[]; revealed: boolean }) {
  return <><div className="learning-objective"><span>本节目标</span>{item.objective}</div><div className="learning-reading">{item.reading.map((section) => <section key={section.heading}><h3>{section.heading}</h3><p>{section.content}</p></section>)}</div><SourceReferences references={refs} selectedIds={item.knowledge_refs} /><div className="topology-card"><span>拓扑记忆训练</span><p>{item.topology_memory.prompt}</p><div className="topology-nodes">{item.topology_memory.nodes.map((node) => <b key={node}>{node}</b>)}</div>{revealed && <ul>{item.topology_memory.links.map((link) => <li key={link}>{link}</li>)}</ul>}</div><div className="association-card"><span>开放式联想</span>{item.open_associations.map((question) => <p key={question}>↗ {question}</p>)}</div>{revealed && <div className="check-answers">{item.check_questions.map((entry) => <div key={entry.question}><strong>{entry.question}</strong><p>{entry.answer}</p></div>)}</div>}</>;
}

function GuidedContent({ item, hintLevel, revealed, onHint }: { item: GuidedQuestion; hintLevel: number; revealed: boolean; onHint: () => void }) {
  return <><div className="scenario"><span>具体场景</span>{item.scenario}</div><blockquote>{item.question}</blockquote><button className="text-action" onClick={onHint} disabled={hintLevel >= item.hints.length}>再给我一级提示</button>{hintLevel > 0 && <div className="hint-stack">{item.hints.slice(0, hintLevel).map((hint, index) => <p key={hint}><span>提示 {index + 1}</span>{hint}</p>)}</div>}{revealed && <Feedback good={item.answer_framework} warning={item.common_mistakes} />}</>;
}

function ModelContent({ item, revealed }: { item: ModelTask; revealed: boolean }) {
  return <><div className="constraint-row">{item.constraints.map((entry) => <span key={entry}>{entry}</span>)}</div><blockquote>{item.prompt}</blockquote>{revealed && <Feedback good={item.required_outputs} warning={item.verification_questions} />}</>;
}

function CaseContent({ item, revealed }: { item: ProfessionalCase; revealed: boolean }) {
  const rubric = [...item.rubric.diagnosis, ...item.rubric.solution, ...item.rubric.unavoidable_costs];
  return <><div className="case-file"><span>CASE / {item.difficulty}</span><p>{item.background}</p>{item.evidence.map((entry) => <code key={entry}>{entry}</code>)}</div><div className="case-questions">{item.questions.map((question, index) => <p key={question}><b>Q{index + 1}</b>{question}</p>)}</div>{revealed && <Feedback good={rubric} warning={item.rubric.boundaries} />}</>;
}

function Feedback({ good, warning }: { good: string[]; warning: string[] }) {
  return <div className="rubric-grid feedback"><div><h3>回答应覆盖</h3><ul>{good.map((entry) => <li key={entry}>{entry}</li>)}</ul></div><div className="mistakes"><h3>反向核验</h3><ul>{warning.map((entry) => <li key={entry}>{entry}</li>)}</ul></div></div>;
}

function RatingBar({ value, onRate }: { value?: Rating; onRate: (value: Rating) => void }) {
  return <div className="rating-row"><span>不看提示时，我能：</span>{(["again", "hard", "good"] as Rating[]).map((rating) => <button key={rating} className={value === rating ? "selected" : ""} onClick={() => onRate(rating)}>{rating === "again" ? "需要重建" : rating === "hard" ? "部分输出" : "完整输出"}</button>)}</div>;
}

function SourceReferences({ references, selectedIds }: { references: KnowledgeRef[]; selectedIds: string[] }) {
  const items = references.filter((item) => selectedIds.includes(item.id));
  return <details className="source-references"><summary>原文依据 · {items.length} 篇</summary>{items.map((item) => <div className="source-reference" key={item.id}><code>{item.source_id}</code><a target="_blank" rel="noreferrer" href={`/__practice/source?source_id=${encodeURIComponent(item.source_id)}&path=${encodeURIComponent(item.path)}`}>{item.path}</a><small>{item.id}</small></div>)}</details>;
}

function Summary({ session }: { session: PracticeSession }) {
  const ratings = Object.values(session.ratings);
  return <div className="panel result-panel"><div className="result-mark">完成</div><div className="eyebrow">{session.unitTitle}</div><h2>本轮训练已完成</h2><p>“需要重建”和“部分输出”会成为后续复习计划的依据。</p><div className="result-stats"><div><strong>{ratings.length}</strong><span>已自评</span></div><div><strong>{ratings.filter((entry) => entry === "again").length}</strong><span>需要重建</span></div><div><strong>{ratings.filter((entry) => entry === "good").length}</strong><span>完整输出</span></div></div><Link className="button-link primary" to="/">返回大厅</Link></div>;
}

function saveLabel(state: SaveState) {
  return state === "dirty" ? "有改动待保存" : state === "saving" ? "正在保存…" : state === "failed" ? "保存失败" : state === "saved" ? "已自动保存" : "进度已保存";
}

function exportDraft(session: PracticeSession) {
  const link = document.createElement("a");
  link.href = URL.createObjectURL(new Blob([JSON.stringify(session, null, 2)], { type: "application/json" }));
  link.download = `loop-draft-${session.id}.json`;
  link.click();
  URL.revokeObjectURL(link.href);
}
