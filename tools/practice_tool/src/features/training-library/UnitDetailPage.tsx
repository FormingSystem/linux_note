import { useEffect, useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import type { PracticeSession } from "../../shared/types";
import { createSession, findActiveSession, saveSession } from "../../infrastructure/persistence/sessionRepository";
import { catalog, loadUnit } from "./content";

export default function UnitDetailPage() {
  const { unitId } = useParams();
  const navigate = useNavigate();
  const item = catalog.units.find((entry) => entry.id === unitId);
  const [active, setActive] = useState<PracticeSession>();
  useEffect(() => { if (unitId) void findActiveSession(unitId).then(setActive); }, [unitId]);
  if (!item) return <section className="panel"><h1>未找到训练单元</h1><Link to="/library">返回训练库</Link></section>;
  const content = loadUnit(item);
  const start = async () => {
    if (active) return navigate(`/sessions/${active.id}/${active.currentStage}/${active.currentItemId}`);
    const session = createSession(content);
    await saveSession(session);
    navigate(`/sessions/${session.id}/learning/${session.currentItemId}`);
  };

  return (
    <section className="hero panel">
      <div className="eyebrow">{item.domain} / {item.topic} / {item.module}</div>
      <h1>{content.unit.title}</h1><p className="hero-subtitle">{content.unit.subtitle}</p>
      <div className="phase-cards">
        {Object.entries(content.unit.stages).map(([key, stage], index) => (
          <div key={key}><span>0{index} · {key}</span><strong>{stage.title}</strong><p>{stage.purpose}</p></div>
        ))}
      </div>
      <div className="notice"><span>知识来源</span>{content.unit.knowledge_refs.length} 个可追溯原文入口；训练内容为重新提炼，不复制原文。</div>
      <div className="actions home-actions">
        <Link className="button-link secondary" to="/library">返回训练库</Link>
        <button className="primary" onClick={() => void start()}>{active ? "继续上次训练" : "从专题学习开始"} →</button>
      </div>
    </section>
  );
}
