import { useEffect, useState } from "react";
import catalogData from "../banks/index.json";
import runtimeConfig from "virtual:practice-runtime-config";
import type {
  GuidedQuestion,
  ModelTask,
  PracticeUnit,
  ProfessionalCase,
  Rating,
  Review,
  UnitCatalog,
  UnitCatalogItem,
} from "./types";

const catalog = catalogData as UnitCatalog;
const STORAGE_KEY = "loop-knowledge-practice.reviews.v1";
const LEGACY_STORAGE_KEY = "linux-note-practice.reviews.v2";
const unitFiles = import.meta.glob<PracticeUnit>("../banks/**/unit.json", { eager: true, import: "default" });
const guidedFiles = import.meta.glob<GuidedQuestion[]>("../banks/**/guided_questions.json", { eager: true, import: "default" });
const modelFiles = import.meta.glob<ModelTask[]>("../banks/**/model_tasks.json", { eager: true, import: "default" });
const caseFiles = import.meta.glob<ProfessionalCase[]>("../banks/**/professional_cases.json", { eager: true, import: "default" });

function loadUnit(item: UnitCatalogItem) {
  const unitPath = `../banks/${item.unit_file}`;
  const unit = unitFiles[unitPath];
  if (!unit) throw new Error(`训练单元不存在：${item.unit_file}`);
  const directory = unitPath.slice(0, unitPath.lastIndexOf("/") + 1);
  const guided = guidedFiles[`${directory}${unit.stages.guided.items_file}`];
  const models = modelFiles[`${directory}${unit.stages.reconstruction.items_file}`];
  const cases = caseFiles[`${directory}${unit.stages.professional.items_file}`];
  if (!guided || !models || !cases) throw new Error(`训练单元阶段文件不完整：${item.id}`);
  return { unit, guided, models, cases };
}

type Stage = "library" | "home" | "guided" | "model" | "professional" | "done";
const stages: Stage[] = ["library", "home", "guided", "model", "professional", "done"];
const labels = ["选择单元", "单元概览", "提示提问", "脱稿输出", "专业案例", "训练总结"];

function loadReviews(): Review[] {
  try {
    const current = localStorage.getItem(STORAGE_KEY);
    if (current) return JSON.parse(current) as Review[];
    const legacy = localStorage.getItem(LEGACY_STORAGE_KEY);
    if (!legacy) return [];
    localStorage.setItem(STORAGE_KEY, legacy);
    return JSON.parse(legacy) as Review[];
  } catch {
    return [];
  }
}

function App() {
  const [stage, setStage] = useState<Stage>("library");
  const [selectedUnit, setSelectedUnit] = useState<UnitCatalogItem | null>(null);
  const [content, setContent] = useState(() => loadUnit(catalog.units[0]));
  const [itemIndex, setItemIndex] = useState(0);
  const [answers, setAnswers] = useState<Record<string, string>>({});
  const [ratings, setRatings] = useState<Record<string, Rating>>({});
  const [hintLevel, setHintLevel] = useState(0);
  const [revealed, setRevealed] = useState(false);
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(null);
  const [updateBusy, setUpdateBusy] = useState(false);
  const [updateActionError, setUpdateActionError] = useState("");
  const stageIndex = stages.indexOf(stage);
  const { unit, guided, models, cases } = content;

  const refreshSystemInfo = async () => {
    const response = await fetch("/__practice/system", { cache: "no-store" });
    if (response.ok) setSystemInfo(await response.json() as SystemInfo);
  };

  const runUpdateAction = async (action: "check" | "apply") => {
    setUpdateBusy(true);
    setUpdateActionError("");
    try {
      const response = await fetch(`/__practice/update/${action}`, {
        method: "POST",
        headers: { "x-practice-token": runtimeConfig.system_api_token },
      });
      const body = await response.json();
      if (!response.ok) throw new Error(body.error || "操作失败");
      await refreshSystemInfo();
    } catch (error) {
      setUpdateActionError(error instanceof Error ? error.message : "操作失败");
    } finally {
      setUpdateBusy(false);
    }
  };

  useEffect(() => {
    void refreshSystemInfo();
    const timer = window.setInterval(() => void refreshSystemInfo(), 60_000);
    return () => window.clearInterval(timer);
  }, []);

  const setAnswer = (id: string, value: string) => setAnswers({ ...answers, [id]: value });
  const moveTo = (next: Stage) => {
    setStage(next);
    setItemIndex(0);
    setHintLevel(0);
    setRevealed(false);
    window.scrollTo({ top: 0, behavior: "smooth" });
  };

  const save = () => {
    const review: Review = {
      unitId: unit.id,
      createdAt: new Date().toISOString(),
      guidedAnswers: Object.fromEntries(guided.map((item) => [item.id, answers[item.id] || ""])),
      modelAnswers: Object.fromEntries(models.map((item) => [item.id, answers[item.id] || ""])),
      caseAnswers: Object.fromEntries(cases.map((item) => [item.id, answers[item.id] || ""])),
      ratings,
    };
    localStorage.setItem(STORAGE_KEY, JSON.stringify([review, ...loadReviews()]));
    moveTo("done");
  };

  const itemCount = guided.length + models.length + cases.length;
  const ratedCount = Object.keys(ratings).length;

  return (
    <div className="app-shell">
      <header className="topbar">
        <div className="brand">
          <span className="brand-mark">回</span>
          <div><strong>回路</strong><span>Knowledge Practice</span></div>
        </div>
          <div className="session-meta">
            <span className="status-dot" />
            本地训练 · {runtimeConfig.sources.length ? `${runtimeConfig.sources.length} 个知识源` : "未配置知识源"}
          </div>
      </header>
      {systemInfo?.update.status === "available" && (
        <div className="update-notice" role="status">
          <span>发现 {systemInfo.update.behind_count} 个更新 · 当前版本 {systemInfo.release.version}</span>
          <button disabled={updateBusy} onClick={() => void runUpdateAction("apply")}>
            {updateBusy ? "更新中…" : "安全更新"}
          </button>
        </div>
      )}
      {systemInfo?.update.status === "updated" && (
        <div className="update-notice" role="status">
          <span>{systemInfo.update.message}</span>
        </div>
      )}
      {updateActionError && (
        <div className="update-notice update-error" role="status">
          <span>更新操作未完成：{updateActionError}</span>
          <button disabled={updateBusy} onClick={() => void runUpdateAction("check")}>重试</button>
        </div>
      )}

      <main>
        <aside className="rail" aria-label="训练阶段">
          <div className="rail-title">{selectedUnit ? `${selectedUnit.module.toUpperCase()} · ${selectedUnit.level}` : "知识训练网络"}</div>
          {labels.map((label, index) => (
            <div className={`rail-step ${index === stageIndex ? "active" : ""} ${index < stageIndex ? "complete" : ""}`} key={label}>
              <span>{index < stageIndex ? "✓" : index}</span>{label}
            </div>
          ))}
          <div className="rail-card">
            <span>三阶段训练</span>
            <strong>{unit.estimated_minutes} 分钟</strong>
            <small>{selectedUnit ? `${itemCount} 个训练任务 · ${unit.knowledge_refs.length} 篇知识来源` : `${catalog.units.length} 个可用单元`}</small>
          </div>
        </aside>

        <section className="workspace">
          {stage === "library" && (
            <UnitLibrary
              units={catalog.units}
              onSelect={(selected) => {
                setSelectedUnit(selected);
                setContent(loadUnit(selected));
                moveTo("home");
              }}
            />
          )}
          {stage === "home" && <Home unit={unit} onStart={() => moveTo("guided")} onBack={() => moveTo("library")} />}
          {stage === "guided" && (
            <GuidedStage
              item={guided[itemIndex]}
              index={itemIndex}
              total={guided.length}
              answer={answers[guided[itemIndex].id] || ""}
              rating={ratings[guided[itemIndex].id]}
              hintLevel={hintLevel}
              revealed={revealed}
              onAnswer={setAnswer}
              onRate={(value) => setRatings({ ...ratings, [guided[itemIndex].id]: value })}
              onHint={() => setHintLevel(Math.min(hintLevel + 1, guided[itemIndex].hints.length))}
              onReveal={() => setRevealed(true)}
              onNext={() => {
                if (itemIndex < guided.length - 1) {
                  setItemIndex(itemIndex + 1); setHintLevel(0); setRevealed(false);
                } else moveTo("model");
              }}
            />
          )}
          {stage === "model" && (
            <ModelStage
              item={models[itemIndex]}
              index={itemIndex}
              total={models.length}
              answer={answers[models[itemIndex].id] || ""}
              revealed={revealed}
              rating={ratings[models[itemIndex].id]}
              onAnswer={setAnswer}
              onReveal={() => setRevealed(true)}
              onRate={(value) => setRatings({ ...ratings, [models[itemIndex].id]: value })}
              onNext={() => {
                if (itemIndex < models.length - 1) {
                  setItemIndex(itemIndex + 1); setRevealed(false);
                } else moveTo("professional");
              }}
            />
          )}
          {stage === "professional" && (
            <ProfessionalStage
              item={cases[itemIndex]}
              index={itemIndex}
              total={cases.length}
              answer={answers[cases[itemIndex].id] || ""}
              revealed={revealed}
              rating={ratings[cases[itemIndex].id]}
              onAnswer={setAnswer}
              onReveal={() => setRevealed(true)}
              onRate={(value) => setRatings({ ...ratings, [cases[itemIndex].id]: value })}
              onNext={() => itemIndex < cases.length - 1 ? (setItemIndex(itemIndex + 1), setRevealed(false)) : save()}
            />
          )}
          {stage === "done" && (
            <div className="panel result-panel">
              <div className="result-mark">完成</div>
              <div className="eyebrow">{unit.title} · 本轮结束</div>
              <h2>你已经走完“辅助建立—脱稿重建—工程迁移”</h2>
              <p>评分不是结论，而是下一轮训练的路由。优先重做“需要重建”的小模块，再回到完整模型输出。</p>
              <div className="result-stats">
                <div><strong>{ratedCount}</strong><span>已自评任务</span></div>
                <div><strong>{Object.values(ratings).filter((v) => v === "again").length}</strong><span>需要重建</span></div>
                <div><strong>{loadReviews().length}</strong><span>本地训练轮次</span></div>
              </div>
              <button className="primary" onClick={() => location.reload()}>开始新一轮</button>
            </div>
          )}
        </section>
      </main>
    </div>
  );
}

function UnitLibrary({ units, onSelect }: { units: UnitCatalogItem[]; onSelect: (unit: UnitCatalogItem) => void }) {
  const [query, setQuery] = useState("");
  const [domain, setDomain] = useState("all");
  const domains = [...new Set(units.map((item) => item.domain))];
  const normalized = query.trim().toLowerCase();
  const filtered = units.filter((item) => {
    const matchesDomain = domain === "all" || item.domain === domain;
    const haystack = [item.title, item.summary, item.module, ...item.tags].join(" ").toLowerCase();
    return matchesDomain && (!normalized || haystack.includes(normalized));
  });

  return (
    <div className="panel library-panel">
      <div className="eyebrow">Knowledge Practice Network</div>
      <div className="library-heading">
        <div><h1>选择训练单元</h1><p>单元负责组织训练，知识结论仍以仓库正文为唯一权威来源。</p></div>
        <div className="catalog-count"><strong>{units.length}</strong><span>可用单元</span></div>
      </div>
      <div className="library-tools">
        <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索模块、知识点或标签……" aria-label="搜索训练单元" />
        <select value={domain} onChange={(e) => setDomain(e.target.value)} aria-label="选择知识领域">
          <option value="all">全部领域</option>
          {domains.map((item) => <option key={item} value={item}>{item}</option>)}
        </select>
      </div>
      <div className="unit-list">
        {filtered.map((item) => (
          <article className="unit-card" key={item.id}>
            <div className="unit-card-top"><span>{item.domain} / {item.topic} / {item.module}</span><b>{item.status === "available" ? "可训练" : item.status}</b></div>
            <h2>{item.title}</h2>
            <p>{item.summary}</p>
            <div className="unit-tags">{item.tags.map((tag) => <span key={tag}>{tag}</span>)}</div>
            <div className="unit-card-footer"><small>{item.estimated_minutes} 分钟 · {item.level}</small><button className="primary" onClick={() => onSelect(item)}>选择此单元 <span>→</span></button></div>
          </article>
        ))}
        {filtered.length === 0 && <div className="empty-state">没有匹配的训练单元。可以清空搜索条件后重试。</div>}
      </div>
    </div>
  );
}

function Home({ unit, onStart, onBack }: { unit: PracticeUnit; onStart: () => void; onBack: () => void }) {
  const sourceIds = [...new Set(unit.knowledge_refs.map((item) => item.source_id))];
  const sourceNames = sourceIds.map((id) => runtimeConfig.sources.find((source) => source.id === id)?.title || `${id}（未配置）`);
  return (
    <div className="hero panel">
      <div className="eyebrow">知识训练单元 · {unit.id}</div>
      <h1>{unit.title}</h1>
      <p className="hero-subtitle">{unit.subtitle}</p>
      <div className="rule" />
      <div className="phase-cards">
        <div><span>01 · 建模</span><strong>{unit.stages.guided.title}</strong><p>{unit.stages.guided.purpose}</p></div>
        <div><span>02 · 提取</span><strong>{unit.stages.reconstruction.title}</strong><p>{unit.stages.reconstruction.purpose}</p></div>
        <div><span>03 · 迁移</span><strong>{unit.stages.professional.title}</strong><p>{unit.stages.professional.purpose}</p></div>
      </div>
      <div className="notice"><span>训练原则</span>先尝试回答，再逐级打开提示；看到提示后想起来，不等于脱稿掌握。</div>
      <div className="notice"><span>知识来源</span>{sourceNames.join("、")}</div>
      <div className="actions home-actions">
        <button className="secondary" onClick={onBack}>重新选择单元</button>
        <button className="primary" onClick={onStart}>从轻量提问开始 <span>→</span></button>
      </div>
    </div>
  );
}

type CommonProps = {
  index: number; total: number; answer: string; revealed: boolean; rating?: Rating;
  onAnswer: (id: string, value: string) => void; onReveal: () => void;
  onRate: (value: Rating) => void; onNext: () => void;
};

function StageHeader({ eyebrow, title, index, total }: { eyebrow: string; title: string; index: number; total: number }) {
  return <div className="panel-heading"><div><div className="eyebrow">{eyebrow}</div><h2>{title}</h2></div><div className="point-count">{index + 1} / {total}</div></div>;
}

function AnswerBox({ id, value, onAnswer, placeholder }: { id: string; value: string; onAnswer: CommonProps["onAnswer"]; placeholder: string }) {
  return <textarea className="answer-box" value={value} onChange={(e) => onAnswer(id, e.target.value)} placeholder={placeholder} />;
}

function RatingBar({ value, onRate }: { value?: Rating; onRate: (value: Rating) => void }) {
  return (
    <div className="rating-row"><span>不看提示时，我能：</span>
      {(["again", "hard", "good"] as Rating[]).map((rating) => (
        <button key={rating} className={value === rating ? "selected" : ""} onClick={() => onRate(rating)}>
          {rating === "again" ? "需要重建" : rating === "hard" ? "部分输出" : "完整输出"}
        </button>
      ))}
    </div>
  );
}

function GuidedStage(props: CommonProps & {
  item: GuidedQuestion; hintLevel: number; onHint: () => void;
}) {
  const { item } = props;
  return (
    <div className="panel">
      <StageHeader eyebrow="阶段 1 · 辅助模型建立" title={item.title} index={props.index} total={props.total} />
      <div className="scenario"><span>具体场景</span>{item.scenario}</div>
      <blockquote>{item.question}</blockquote>
      <AnswerBox id={item.id} value={props.answer} onAnswer={props.onAnswer} placeholder="先用自己的话回答。此阶段允许答案短小，但要说明原因……" />
      {props.hintLevel > 0 && <div className="hint-stack">{item.hints.slice(0, props.hintLevel).map((h, i) => <p key={h}><span>提示 {i + 1}</span>{h}</p>)}</div>}
      {props.revealed && <Feedback title="模型骨架" good={item.answer_framework} warning={item.common_mistakes} />}
      <RatingBar value={props.rating} onRate={props.onRate} />
      <div className="actions">
        <button className="secondary" onClick={props.onHint} disabled={props.hintLevel >= item.hints.length}>逐级提示</button>
        {!props.revealed ? <button className="secondary" onClick={props.onReveal}>对照模型骨架</button> : <button className="primary" onClick={props.onNext}>下一题 <span>→</span></button>}
      </div>
    </div>
  );
}

function ModelStage(props: CommonProps & { item: ModelTask }) {
  const { item } = props;
  return (
    <div className="panel">
      <StageHeader eyebrow="阶段 2 · 模型脱稿输出" title={item.title} index={props.index} total={props.total} />
      <div className="constraint-row">{item.constraints.map((c) => <span key={c}>{c}</span>)}</div>
      <blockquote>{item.prompt}</blockquote>
      <AnswerBox id={item.id} value={props.answer} onAnswer={props.onAnswer} placeholder="这里没有知识提示。请写出角色、状态位置、动作顺序和可观察后果……" />
      {props.revealed && <Feedback title="必要输出" good={item.required_outputs} warning={item.verification_questions} warningTitle="反向核验" />}
      <RatingBar value={props.rating} onRate={props.onRate} />
      <div className="actions">
        <span className="quiet">提交前不要查阅正文</span>
        {!props.revealed ? <button className="secondary" onClick={props.onReveal}>完成作答并核验</button> : <button className="primary" onClick={props.onNext}>下一任务 <span>→</span></button>}
      </div>
    </div>
  );
}

function ProfessionalStage(props: CommonProps & { item: ProfessionalCase }) {
  const { item } = props;
  const rubric = [...item.rubric.diagnosis, ...item.rubric.solution, ...item.rubric.unavoidable_costs];
  return (
    <div className="panel">
      <StageHeader eyebrow="阶段 3 · 专业知识体系提问" title={item.title} index={props.index} total={props.total} />
      <div className="case-file"><span>CASE / {item.difficulty}</span><p>{item.background}</p>{item.evidence.map((e) => <code key={e}>{e}</code>)}</div>
      <div className="case-questions">{item.questions.map((q, i) => <p key={q}><b>Q{i + 1}</b>{q}</p>)}</div>
      <AnswerBox id={item.id} value={props.answer} onAnswer={props.onAnswer} placeholder="给出诊断、机制解释、解决办法、无法规避的成本，以及不应使用该方案的条件……" />
      {props.revealed && <Feedback title="专业回答应覆盖" good={rubric} warning={item.rubric.boundaries} warningTitle="选择边界" />}
      <RatingBar value={props.rating} onRate={props.onRate} />
      <div className="actions">
        <span className="quiet">判断“不可规避成本”是本阶段的必答项</span>
        {!props.revealed ? <button className="secondary" onClick={props.onReveal}>提交方案并看评审点</button> : <button className="primary" onClick={props.onNext}>{props.index + 1 === props.total ? "完成本轮" : "下一个案例"} <span>→</span></button>}
      </div>
    </div>
  );
}

function Feedback({ title, good, warning, warningTitle = "常见失真" }: { title: string; good: string[]; warning: string[]; warningTitle?: string }) {
  return <div className="rubric-grid feedback"><div><h3>{title}</h3><ul>{good.map((x) => <li key={x}>{x}</li>)}</ul></div><div className="mistakes"><h3>{warningTitle}</h3><ul>{warning.map((x) => <li key={x}>{x}</li>)}</ul></div></div>;
}

export default App;

type SystemInfo = {
  release: { version: string; channel: string; editable: false };
  security: { profile: string; bind_host: string; editable: false };
  update: {
    status: "idle" | "checking" | "current" | "available" | "error" | "updated";
    behind_count: number;
    message: string;
  };
};
