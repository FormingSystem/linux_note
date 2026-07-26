import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import type { PracticeSession } from "../../shared/types";
import { listSessions } from "../../infrastructure/persistence/sessionRepository";
import { catalog } from "../training-library/content";

export default function LobbyPage() {
  const [sessions, setSessions] = useState<PracticeSession[]>([]);
  useEffect(() => { void listSessions().then(setSessions); }, []);
  const active = sessions.filter((item) => item.status === "in_progress").slice(0, 3);

  return (
    <div className="page-stack">
      <section className="lobby-hero panel">
        <div>
          <div className="eyebrow">Knowledge Practice Network</div>
          <h1>今天从哪里继续？</h1>
          <p>从提炼后的专题电子书开始，再经过提示提问、脱稿输出与专业案例，把知识变成可迁移的模型。</p>
        </div>
        <Link className="button-link primary" to="/library">浏览训练库 →</Link>
      </section>

      <section className="dashboard-section">
        <div className="section-heading"><div><h2>继续训练</h2><p>进度保存在本机，刷新或关闭页面后仍可继续。</p></div></div>
        <div className="dashboard-grid">
          {active.map((session) => (
            <article className="dashboard-card" key={session.id}>
              <span>{session.currentStage}</span><h3>{session.unitTitle}</h3>
              <p>上次训练：{new Date(session.updatedAt).toLocaleString()}</p>
              <Link to={`/sessions/${session.id}/${session.currentStage}/${session.currentItemId}`}>继续训练 →</Link>
            </article>
          ))}
          {!active.length && <div className="empty-state">还没有进行中的训练。先从下方推荐专题开始。</div>}
        </div>
      </section>

      <section className="dashboard-section">
        <div className="section-heading"><div><h2>推荐专题</h2><p>这里完全来自题库索引，新增专题后会自动出现。</p></div><Link to="/library">查看全部</Link></div>
        <div className="dashboard-grid">
          {catalog.units.filter((item) => item.status === "available").slice(0, 3).map((unit) => (
            <article className="dashboard-card" key={unit.id}>
              <span>{unit.domain} / {unit.module}</span><h3>{unit.title}</h3><p>{unit.summary}</p>
              <Link to={`/library/units/${unit.id}`}>查看单元 →</Link>
            </article>
          ))}
        </div>
      </section>
    </div>
  );
}
