import { useEffect, useMemo, useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import type { ImportedBook } from "../../shared/types";
import { loadImportedBook } from "../../infrastructure/persistence/importedBookRepository";
import MarkdownGuide, { markdownHeadings } from "../practice-sessions/components/MarkdownGuide";

export default function ImportedBookReaderPage() {
  const { bookId = "", chapterId = "" } = useParams();
  const navigate = useNavigate();
  const [book, setBook] = useState<ImportedBook>();
  const [error, setError] = useState("");
  const [leftOpen, setLeftOpen] = useState(true);
  const [rightOpen, setRightOpen] = useState(true);
  useEffect(() => { void loadImportedBook(bookId).then((value) => value ? setBook(value) : setError("电子书不存在或已被清理。")); }, [bookId]);
  const chapter = book?.chapters.find((item) => item.id === chapterId) ?? book?.chapters[0];
  useEffect(() => {
    if (book && chapter && chapterId !== chapter.id) navigate(`/library/books/${book.id}/${chapter.id}`, { replace: true });
  }, [book, chapter, chapterId, navigate]);
  useEffect(() => {
    if (!chapter) return;
    const hash = decodeURIComponent(window.location.hash.slice(1));
    window.requestAnimationFrame(() => {
      if (hash) document.getElementById(hash)?.scrollIntoView({ block: "start" });
      else window.scrollTo({ top: 0 });
    });
  }, [chapter]);
  useEffect(() => {
    const restoreHashPosition = () => {
      const hash = decodeURIComponent(window.location.hash.slice(1));
      if (hash) window.requestAnimationFrame(() => document.getElementById(hash)?.scrollIntoView({ block: "start" }));
    };
    window.addEventListener("popstate", restoreHashPosition);
    return () => window.removeEventListener("popstate", restoreHashPosition);
  }, []);
  const headings = useMemo(() => chapter ? markdownHeadings(chapter.markdown) : [], [chapter]);
  if (error) return <section className="panel error-state"><h1>无法打开电子书</h1><p>{error}</p><Link to="/library/import">返回电子书导入器</Link></section>;
  if (!book || !chapter) return <section className="panel loading-state">正在打开电子书……</section>;
  const index = book.chapters.findIndex((item) => item.id === chapter.id);
  const selectChapter = (targetChapterId: string) => {
    navigate(`/library/books/${book.id}/${targetChapterId}`);
  };
  const jumpToHeading = (headingId: string) => {
    const target = document.getElementById(headingId);
    if (!target) return;
    window.history.pushState(null, "", `${window.location.pathname}${window.location.search}#${encodeURIComponent(headingId)}`);
    target.scrollIntoView({ behavior: "smooth", block: "start" });
  };
  return <section className="imported-book-reader">
    <header><div><span>{book.mode === "source" ? "原文阅读" : "专题电子书"} · {book.status === "published" ? "已发布" : "草稿"}</span><h1>{book.title}</h1><small>版本 {book.version} · 来源 {book.sourceId}</small></div><Link to="/library/import">返回书库</Link></header>
    <div className={`ebook-reader imported-reader-grid ${leftOpen ? "" : "left-collapsed"} ${rightOpen ? "" : "right-collapsed"}`}>
      <aside className={leftOpen ? "book-navigation reader-sidebar left-sidebar" : "reader-sidebar left-sidebar collapsed"}>
        <button className="reader-sidebar-toggle" aria-label={leftOpen ? "收起章节目录" : "展开章节目录"} title={leftOpen ? "收起章节目录" : "展开章节目录"} onClick={() => setLeftOpen((value) => !value)}>{leftOpen ? "‹" : "›"}</button>
        {leftOpen && <><div className="book-navigation-heading"><span>章节目录</span><strong>{book.title}</strong></div><ol>{book.chapters.map((item, itemIndex) => <li key={item.id}><button className={item.id === chapter.id ? "active" : ""} onClick={() => selectChapter(item.id)}><span>{String(itemIndex + 1).padStart(2, "0")}</span><span><strong>{item.title}</strong><small>{item.sourceName}</small></span></button></li>)}</ol></>}
      </aside>
      <main className="ebook-content">
        <details className="book-outline"><summary>全书导读</summary><MarkdownGuide markdown={book.outlineMarkdown} /></details>
        {book.mode === "topic" && <div className="learning-objective"><span>本章目标</span>{chapter.objective}</div>}
        <MarkdownGuide markdown={chapter.markdown} headings={headings} />
        {book.mode === "topic" && <><section className="chapter-claims"><span>候选知识声明</span><div><strong>{chapter.candidateClaim}</strong><small>{chapter.confirmed ? "已人工确认" : "尚未确认"}</small></div></section><div className="topology-card"><span>训练任务草稿</span><p>{chapter.trainingPrompt}</p></div></>}
        <nav className="chapter-pagination">{index > 0 ? <button onClick={() => selectChapter(book.chapters[index - 1].id)}><small>上一章</small><strong>← {book.chapters[index - 1].title}</strong></button> : <span />}{index < book.chapters.length - 1 && <button onClick={() => selectChapter(book.chapters[index + 1].id)}><small>下一章</small><strong>{book.chapters[index + 1].title} →</strong></button>}</nav>
      </main>
      <nav className={rightOpen ? "chapter-toc reader-sidebar right-sidebar" : "reader-sidebar right-sidebar collapsed"}>
        <button className="reader-sidebar-toggle" aria-label={rightOpen ? "收起本章目录" : "展开本章目录"} title={rightOpen ? "收起本章目录" : "展开本章目录"} onClick={() => setRightOpen((value) => !value)}>{rightOpen ? "›" : "‹"}</button>
        {rightOpen && <><strong>本章目录</strong>{headings.map((heading) => <a className={`level-${heading.level}`} href={`#${encodeURIComponent(heading.id)}`} key={heading.id} onClick={(event) => { event.preventDefault(); jumpToHeading(heading.id); }}>{heading.title}</a>)}</>}
      </nav>
    </div>
  </section>;
}
