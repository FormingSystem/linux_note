import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import type {
  GuidedQuestion, KnowledgeRef, LearningChapter, ModelTask, PracticeSession,
  ProfessionalCase, Rating, SaveState, TrainingStage,
} from "../../../shared/types";
import { loadSession, saveSession, stageItems } from "../../../infrastructure/persistence/sessionRepository";
import StageRail from "../components/StageRail";
import MarkdownGuide from "../components/MarkdownGuide";
import BookNavigation from "../components/BookNavigation";
import { STAGES } from "../model/stages";

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
      const fallback = session.stagePositions[validStage] ?? stageItems(session, validStage)[0]?.id;
      if (fallback) navigate(`/sessions/${session.id}/${validStage}/${fallback}`, { replace: true });
    }
  }, [index, navigate, session, validStage]);

  if (loadError) return <section className="panel error-state"><h1>无法继续训练</h1><p>{loadError}</p><Link to="/">返回大厅</Link></section>;
  if (!session) return <section className="panel loading-state">正在恢复训练进度……</section>;
  if (!validStage) return <RedirectToCurrent session={session} />;

  const completed = new Set(session.completedItemIds);
  const goStage = (target: TrainingStage) => {
    const targetId = target === "summary" ? "complete" : session.stagePositions[target] ?? stageItems(session, target)[0]?.id;
    if (targetId) routeTo(target, targetId);
  };
  const routeTo = (targetStage: TrainingStage, targetItemId: string) => {
    update((current) => ({ ...current, currentStage: targetStage, currentItemId: targetItemId, stagePositions: { ...current.stagePositions, [targetStage]: targetItemId } }));
    navigate(`/sessions/${session.id}/${targetStage}/${targetItemId}`);
    window.scrollTo({ top: 0, behavior: "smooth" });
  };
  const finishItem = () => {
    if (!session.ratings[itemId]) return;
    const nextCompleted = [...new Set([...session.completedItemIds, itemId])];
    const nextItem = currentItems[index + 1];
    if (nextItem) {
      update((current) => ({ ...current, completedItemIds: nextCompleted, currentStage: validStage, currentItemId: nextItem.id, stagePositions: { ...current.stagePositions, [validStage]: nextItem.id } }));
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
      stagePositions: { ...current.stagePositions, [nextStage]: nextId },
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
      <StageRail session={session} activeStage={validStage} completed={completed} saveState={saveState} onSelect={goStage} />
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
            onSelectChapter={(chapterId) => routeTo("learning", chapterId)}
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
  onPrevious: () => void; onNext: () => void; onSelectChapter: (chapterId: string) => void;
};

function TrainingItem({ session, stage, itemId, index, total, onUpdate, onPrevious, onNext, onSelectChapter }: ItemProps) {
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
      {stage === "learning" && <LearningContent item={item as LearningChapter} session={session} refs={content.unit.knowledge_refs} revealed={revealed} onSelectChapter={onSelectChapter} />}
      {stage === "guided" && <GuidedContent item={item as GuidedQuestion} hintLevel={session.hintLevels[itemId] ?? 0} revealed={revealed} onHint={() => onUpdate((current) => ({ ...current, hintLevels: { ...current.hintLevels, [itemId]: Math.min((current.hintLevels[itemId] ?? 0) + 1, (item as GuidedQuestion).hints.length) } }))} />}
      {stage === "reconstruction" && <ModelContent item={item as ModelTask} revealed={revealed} />}
      {stage === "professional" && <CaseContent item={item as ProfessionalCase} revealed={revealed} />}
      {stage !== "learning" && revealed && <RelatedChapters session={session} chapterIds={(item as GuidedQuestion | ModelTask | ProfessionalCase).chapter_ids} onSelect={onSelectChapter} />}
      <textarea className="answer-box" value={answer} onChange={(event) => setAnswer(event.target.value)} placeholder="用自己的话完成回答。内容会自动保存在本机……" />
      <RatingBar value={rating} onRate={rate} />
      <div className="actions">
        <button className="secondary" onClick={onPrevious} disabled={stage === "learning" && index === 0}>← 上一项</button>
        {!revealed ? <button className="secondary" onClick={reveal}>查看答案与核验点</button> : <button className="primary" disabled={!rating} onClick={onNext}>完成并继续 →</button>}
      </div>
    </div>
  );
}

function RelatedChapters({ session, chapterIds, onSelect }: { session: PracticeSession; chapterIds: string[]; onSelect: (chapterId: string) => void }) {
  const chapters = session.contentSnapshot.book.chapters.filter((chapter) => chapterIds.includes(chapter.id));
  return <section className="related-chapters"><span>建议回看</span>{chapters.map((chapter) => <button key={chapter.id} onClick={() => onSelect(chapter.id)}>{chapter.title} →</button>)}</section>;
}

function LearningContent({ item, session, refs, revealed, onSelectChapter }: { item: LearningChapter; session: PracticeSession; refs: KnowledgeRef[]; revealed: boolean; onSelectChapter: (chapterId: string) => void }) {
  const claims = session.contentSnapshot.book.claims.filter((claim) => item.claim_ids.includes(claim.id));
  return <><BookNavigation book={session.contentSnapshot.book} activeChapterId={item.id} completed={new Set(session.completedItemIds)} onSelect={(chapter) => onSelectChapter(chapter.id)} /><div className="learning-objective"><span>本章目标</span>{item.objective}</div><MarkdownGuide markdown={item.content_markdown} />{claims.length > 0 && <section className="chapter-claims"><span>本章知识声明</span>{claims.map((claim) => <div key={claim.id}><strong>{claim.statement}</strong><small>{claim.status} · {claim.id}</small></div>)}</section>}<SourceReferences references={refs} selectedIds={item.knowledge_refs} /><div className="topology-card"><span>拓扑记忆训练</span><p>{item.topology_memory.prompt}</p><div className="topology-nodes">{item.topology_memory.nodes.map((node) => <b key={node}>{node}</b>)}</div>{revealed && <ul>{item.topology_memory.links.map((link) => <li key={link}>{link}</li>)}</ul>}</div><div className="association-card"><span>开放式联想</span>{item.open_associations.map((question) => <p key={question}>↗ {question}</p>)}</div>{revealed && <div className="check-answers">{item.check_questions.map((entry) => <div key={entry.question}><strong>{entry.question}</strong><p>{entry.answer}</p></div>)}</div>}</>;
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
  const done = session.status === "completed";
  return <div className="panel result-panel"><div className="result-mark">{done ? "完成" : "进度"}</div><div className="eyebrow">{session.unitTitle}</div><h2>{done ? "本轮训练已完成" : "当前训练进度"}</h2><p>{done ? "“需要重建”和“部分输出”会成为后续复习计划的依据。" : "总结页可以随时查看，但只有完成全部训练项后才会标记本轮完成。"}</p><div className="result-stats"><div><strong>{ratings.length}</strong><span>已自评</span></div><div><strong>{ratings.filter((entry) => entry === "again").length}</strong><span>需要重建</span></div><div><strong>{ratings.filter((entry) => entry === "good").length}</strong><span>完整输出</span></div></div><Link className="button-link primary" to="/">返回大厅</Link></div>;
}

function exportDraft(session: PracticeSession) {
  const link = document.createElement("a");
  link.href = URL.createObjectURL(new Blob([JSON.stringify(session, null, 2)], { type: "application/json" }));
  link.download = `loop-draft-${session.id}.json`;
  link.click();
  URL.revokeObjectURL(link.href);
}
