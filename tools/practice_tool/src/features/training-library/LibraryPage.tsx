import { useMemo, useState } from "react";
import { Link } from "react-router-dom";
import { catalog } from "./content";

export default function LibraryPage() {
  const [query, setQuery] = useState("");
  const [domain, setDomain] = useState("all");
  const domains = [...new Set(catalog.units.map((item) => item.domain))];
  const units = useMemo(() => {
    const normalized = query.trim().toLowerCase();
    return catalog.units.filter((item) => {
      const haystack = [item.title, item.summary, item.module, ...item.tags].join(" ").toLowerCase();
      return item.status === "available" && (domain === "all" || item.domain === domain) && (!normalized || haystack.includes(normalized));
    });
  }, [domain, query]);

  return (
    <section className="panel library-panel">
      <div className="library-heading">
        <div><div className="eyebrow">Training Library</div><h1>训练库</h1><p>先了解单元目标和来源，再决定开始或继续训练。</p></div>
        <Link className="button-link secondary" to="/my-training">管理分类与模块</Link>
      </div>
      <div className="library-tools">
        <input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="搜索模块、知识点或标签……" aria-label="搜索训练单元" />
        <select value={domain} onChange={(event) => setDomain(event.target.value)} aria-label="选择知识领域">
          <option value="all">全部领域</option>
          {domains.map((item) => <option key={item}>{item}</option>)}
        </select>
      </div>
      <div className="unit-list">
        {units.map((item) => (
          <article className="unit-card" key={item.id}>
            <div className="unit-card-top"><span>{item.domain} / {item.topic} / {item.module}</span><b>可训练</b></div>
            <h2>{item.title}</h2><p>{item.summary}</p>
            <div className="unit-tags">{item.tags.map((tag) => <span key={tag}>{tag}</span>)}</div>
            <div className="unit-card-footer"><small>{item.estimated_minutes} 分钟 · {item.level}</small><Link className="button-link primary" to={`/library/units/${item.id}`}>查看详情 →</Link></div>
          </article>
        ))}
        {!units.length && <div className="empty-state">没有匹配的训练单元。</div>}
      </div>
    </section>
  );
}
