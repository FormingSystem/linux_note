import { ChangeEvent, useEffect, useMemo, useRef, useState } from "react";
import { Link, useNavigate, useSearchParams } from "react-router-dom";
import runtimeConfig from "virtual:practice-runtime-config";
import type { ImportedBook, ImportedBookChapter, ImportedBookMode } from "../../shared/types";
import { deleteImportedBook, listImportedBooks, loadImportedBook, saveImportedBook } from "../../infrastructure/persistence/importedBookRepository";
import MarkdownGuide, { markdownPlainText } from "../practice-sessions/components/MarkdownGuide";

const makeId = () => crypto.randomUUID();
const naturalPathCollator = new Intl.Collator("zh-CN", {
  numeric: true,
  sensitivity: "base",
});

export default function BookImporterPage() {
  const navigate = useNavigate();
  const [searchParams] = useSearchParams();
  const editingId = searchParams.get("book");
  const [bookId, setBookId] = useState("");
  const [createdAt, setCreatedAt] = useState("");
  const [mode, setMode] = useState<ImportedBookMode>("source");
  const [title, setTitle] = useState("");
  const [version, setVersion] = useState("0.1.0");
  const [sourceId, setSourceId] = useState("local-import");
  const [chapters, setChapters] = useState<ImportedBookChapter[]>([]);
  const [selected, setSelected] = useState(0);
  const [message, setMessage] = useState("");
  const [books, setBooks] = useState<ImportedBook[]>([]);
  const [deleteTarget, setDeleteTarget] = useState("");
  const directoryInput = useRef<HTMLInputElement>(null);
  const confirmAllInput = useRef<HTMLInputElement>(null);

  useEffect(() => {
    directoryInput.current?.setAttribute("webkitdirectory", "");
    directoryInput.current?.setAttribute("directory", "");
    void listImportedBooks().then(setBooks);
    if (!editingId) return;
    void loadImportedBook(editingId).then((book) => {
      if (!book) return setMessage("要编辑的电子书不存在");
      setBookId(book.id);
      setCreatedAt(book.createdAt);
      setMode(book.mode);
      setTitle(book.title);
      setVersion(book.version);
      setSourceId(book.sourceId);
      setChapters(book.chapters);
      setSelected(0);
      setMessage("已载入电子书草稿");
    });
  }, [editingId]);

  const outline = useMemo(() => [
    `# ${title || "未命名电子书"}`,
    "",
    mode === "source" ? "本书按所选原始 Markdown 顺序编排。" : "本书是由权威材料生成的专题提炼草稿，候选结论与训练任务需人工确认。",
    "",
    ...chapters.map((chapter, index) => `${index + 1}. ${chapter.title}`),
  ].join("\n"), [chapters, mode, title]);

  const importFiles = async (event: ChangeEvent<HTMLInputElement>) => {
    const files = [...(event.target.files ?? [])]
      .filter((file) => file.name.toLowerCase().endsWith(".md"))
      .sort((left, right) => {
        const leftPath = (left.webkitRelativePath || left.name).replaceAll("\\", "/");
        const rightPath = (right.webkitRelativePath || right.name).replaceAll("\\", "/");
        return naturalPathCollator.compare(leftPath, rightPath) || (leftPath < rightPath ? -1 : leftPath > rightPath ? 1 : 0);
      });
    const importedDirectory = files
      .map((file) => file.webkitRelativePath.split("/")[0])
      .find((directory) => directory);
    const loaded = await Promise.all(files.map(async (file) => {
      const markdown = await file.text();
      const chapterTitle = extractTitle(markdown) || file.name.replace(/\.md$/i, "");
      const firstParagraph = extractFirstParagraph(markdown);
      return {
        id: makeId(),
        title: chapterTitle,
        sourceName: file.webkitRelativePath || file.name,
        markdown,
        objective: mode === "topic" ? `理解并能够重建“${chapterTitle}”解决的问题、机制和适用边界。` : "",
        candidateClaim: mode === "topic" ? firstParagraph : "",
        trainingPrompt: mode === "topic" ? `不查看原文，说明“${chapterTitle}”中的参与对象、状态位置、因果过程和选择边界。` : "",
        confirmed: false,
      };
    }));
    setChapters(loaded);
    setSelected(0);
    if (importedDirectory) setTitle(importedDirectory);
    else if (!title && loaded[0]) setTitle(loaded.length === 1 ? loaded[0].title : `${loaded[0].title}等专题材料`);
    setMessage(`已读取 ${loaded.length} 个 Markdown 文件`);
    event.target.value = "";
  };

  const updateChapter = (patch: Partial<ImportedBookChapter>) => {
    setChapters((items) => items.map((chapter, index) => index === selected ? { ...chapter, ...patch } : chapter));
  };

  const setChapterConfirmed = (index: number, confirmed: boolean) => {
    setChapters((items) => items.map((chapter, itemIndex) => itemIndex === index ? { ...chapter, confirmed } : chapter));
  };

  const allConfirmed = chapters.length > 0 && chapters.every((chapter) => chapter.confirmed);
  const confirmedCount = chapters.filter((chapter) => chapter.confirmed).length;
  useEffect(() => {
    if (confirmAllInput.current) confirmAllInput.current.indeterminate = confirmedCount > 0 && !allConfirmed;
  }, [allConfirmed, confirmedCount]);
  const setAllConfirmed = (confirmed: boolean) => {
    setChapters((items) => items.map((chapter) => ({ ...chapter, confirmed })));
  };

  const move = (direction: -1 | 1) => {
    const target = selected + direction;
    if (target < 0 || target >= chapters.length) return;
    setChapters((items) => {
      const next = [...items];
      [next[selected], next[target]] = [next[target], next[selected]];
      return next;
    });
    setSelected(target);
  };

  const changeMode = (nextMode: ImportedBookMode) => {
    setMode(nextMode);
    setChapters((items) => items.map((chapter) => nextMode === "source" ? chapter : {
      ...chapter,
      objective: chapter.objective || `理解并能够重建“${chapter.title}”解决的问题、机制和适用边界。`,
      candidateClaim: chapter.candidateClaim || extractFirstParagraph(chapter.markdown),
      trainingPrompt: chapter.trainingPrompt || `不查看原文，说明“${chapter.title}”中的参与对象、状态位置、因果过程和选择边界。`,
      confirmed: false,
    }));
  };

  const validate = (publish: boolean) => {
    const problems: string[] = [];
    if (!title.trim()) problems.push("缺少书名");
    if (!/^\d+\.\d+\.\d+$/.test(version)) problems.push("版本必须使用 x.y.z");
    if (!sourceId.trim()) problems.push("缺少知识源 ID");
    if (!chapters.length) problems.push("至少选择一个 Markdown 文件");
    if (chapters.some((chapter) => !chapter.title.trim() || !chapter.markdown.trim())) problems.push("章节标题或正文为空");
    if (mode === "topic" && chapters.some((chapter) => !chapter.objective.trim() || !chapter.candidateClaim.trim() || !chapter.trainingPrompt.trim())) {
      problems.push("专题章节必须填写目标、候选声明和训练任务");
    }
    if (publish && chapters.some((chapter) => !chapter.confirmed)) problems.push("发布前必须逐章确认全部章节");
    return problems;
  };

  const save = async (publish: boolean) => {
    const problems = validate(publish);
    if (problems.length) return setMessage(`无法${publish ? "发布" : "保存"}：${problems.join("；")}`);
    const now = new Date().toISOString();
    const book: ImportedBook = {
      schemaVersion: 1,
      id: bookId || makeId(),
      title: title.trim(),
      version,
      mode,
      status: publish ? "published" : "draft",
      sourceId: sourceId.trim(),
      outlineMarkdown: outline,
      chapters,
      createdAt: createdAt || now,
      updatedAt: now,
      publishedAt: publish ? now : undefined,
    };
    await saveImportedBook(book);
    if (publish) navigate(`/library/books/${book.id}/${book.chapters[0].id}`);
    else {
      setBooks(await listImportedBooks());
      setMessage("草稿已保存到本机书库");
    }
  };

  const removeBook = async (book: ImportedBook) => {
    await deleteImportedBook(book.id);
    setBooks(await listImportedBooks());
    setDeleteTarget("");
    if (book.id === bookId) {
      setBookId("");
      setCreatedAt("");
      setTitle("");
      setChapters([]);
      setSelected(0);
      navigate("/library/import", { replace: true });
    }
    setMessage(`已删除《${book.title}》`);
  };

  return <section className="book-import-page">
    <header className="import-heading">
      <div><div className="eyebrow">Book Importer</div><h1>电子书导入器</h1><p>原文阅读保留原始正文；专题电子书额外生成候选声明和训练任务草稿，并在发布前逐章确认。</p></div>
      <div className="import-file-actions">
        <label className="file-picker secondary-picker">选择 Markdown 文件<input type="file" accept=".md,text/markdown" multiple onChange={(event) => void importFiles(event)} /></label>
        <label className="file-picker">选择整个目录<input ref={directoryInput} type="file" accept=".md,text/markdown" multiple onChange={(event) => void importFiles(event)} /></label>
      </div>
    </header>
    <div className="import-config-bar">
      <div className="mode-picker" role="group" aria-label="导入模式">
        <button className={mode === "source" ? "selected" : ""} onClick={() => changeMode("source")}><strong>原文阅读</strong><small>原样保存正文，只生成书籍与章节目录</small></button>
        <button className={mode === "topic" ? "selected" : ""} onClick={() => changeMode("topic")}><strong>专题电子书</strong><small>生成目标、候选声明和训练任务草稿</small></button>
      </div>
      <div className="book-settings">
        <label>书名<input value={title} onChange={(event) => setTitle(event.target.value)} /></label>
        <label>版本<input value={version} onChange={(event) => setVersion(event.target.value)} /></label>
        <label>知识源<select value={sourceId} onChange={(event) => setSourceId(event.target.value)}><option value="local-import">本机临时导入</option>{runtimeConfig.sources.map((source) => <option value={source.id} key={source.id}>{source.title}（{source.id}）</option>)}</select></label>
      </div>
    </div>
    <div className="import-workspace">
      <aside className={`import-chapters ${chapters.length > 0 ? "has-action-dock" : ""}`}>
        <div className="chapter-toolbar">
          {chapters.length > 0 ? <label className="confirm-all" title="确认或取消全部章节"><input ref={confirmAllInput} type="checkbox" checked={allConfirmed} onChange={(event) => setAllConfirmed(event.target.checked)} /></label> : <span className="confirm-column-placeholder" />}
          <span className="chapter-toolbar-title"><strong>全书目录</strong><small>已确认 {confirmedCount}/{chapters.length}</small></span>
          <span className="chapter-order-actions"><button onClick={() => move(-1)} disabled={selected === 0}>↑</button><button onClick={() => move(1)} disabled={selected >= chapters.length - 1}>↓</button></span>
        </div>
        <div className="import-chapter-list">
          {chapters.map((chapter, index) => <div className={`import-chapter-row ${selected === index ? "active" : ""}`} key={chapter.id}>
            <label className="chapter-confirm" title={chapter.confirmed ? "本章已确认" : "勾选以确认本章"}><input type="checkbox" checked={chapter.confirmed} onChange={(event) => setChapterConfirmed(index, event.target.checked)} /></label>
            <button onClick={() => setSelected(index)}><b>{String(index + 1).padStart(2, "0")}</b><span>{chapter.title}<small>{chapter.sourceName}</small></span></button>
            <span className={chapter.confirmed ? "chapter-confirm-state confirmed" : "chapter-confirm-state"}>{chapter.confirmed ? "已确认" : "待确认"}</span>
          </div>)}
          {!chapters.length && <p>请选择一个或多个 Markdown 文件。</p>}
        </div>
        {chapters.length > 0 && <div className="import-action-bar">
          <span role="status">{message || `已读取 ${chapters.length} 个章节`}</span>
          <div>
            <button className="secondary" onClick={() => void save(false)}>保存草稿</button>
            <button className="primary" onClick={() => void save(true)}>校验并发布</button>
          </div>
        </div>}
      </aside>
      <main className="import-editor">
        {chapters[selected] ? <>
          <label>章节标题<input value={chapters[selected].title} onChange={(event) => updateChapter({ title: event.target.value })} /></label>
          {mode === "topic" && <>
            <label>本章目标<textarea value={chapters[selected].objective} onChange={(event) => updateChapter({ objective: event.target.value })} /></label>
            <label>候选知识声明<textarea value={chapters[selected].candidateClaim} onChange={(event) => updateChapter({ candidateClaim: event.target.value })} /></label>
            <label>训练任务草稿<textarea value={chapters[selected].trainingPrompt} onChange={(event) => updateChapter({ trainingPrompt: event.target.value })} /></label>
          </>}
          <label className="confirm-chapter"><input type="checkbox" checked={chapters[selected].confirmed} onChange={(event) => updateChapter({ confirmed: event.target.checked })} />我已核对本章内容，可以发布</label>
          <details className="markdown-preview" open><summary>正文预览</summary><MarkdownGuide markdown={chapters[selected].markdown} /></details>
        </> : <div className="empty-state">导入 Markdown 后在这里编辑并预览。</div>}
      </main>
    </div>
    {books.length > 0 && <section className="local-book-list"><h2>本机电子书</h2>{books.map((book) => <article key={book.id}>
      <Link to={`/library/books/${book.id}/${book.chapters[0]?.id ?? ""}`}><span>{book.status === "published" ? "已发布" : "草稿"} · {book.mode === "source" ? "原文阅读" : "专题电子书"}</span><strong>{book.title}</strong><small>{book.chapters.length} 章 · v{book.version}</small></Link>
      <div className="local-book-actions">
        <Link to={`/library/books/${book.id}/${book.chapters[0]?.id ?? ""}`}>打开阅读</Link>
        <Link to={`/library/import?book=${book.id}`}>编辑</Link>
        {deleteTarget === book.id ? <><button className="danger-action" onClick={() => void removeBook(book)}>确认删除</button><button onClick={() => setDeleteTarget("")}>取消</button></> : <button className="danger-action" onClick={() => setDeleteTarget(book.id)}>删除</button>}
      </div>
    </article>)}</section>}
  </section>;
}

function extractTitle(markdown: string) {
  const title = markdown.match(/^#\s+(.+)$/m)?.[1] ?? "";
  return markdownPlainText(title);
}

function extractFirstParagraph(markdown: string) {
  return markdown
    .replace(/^---[\s\S]*?---\s*/m, "")
    .split(/\n\s*\n/)
    .map((part) => part.replace(/^#+\s+.*$/gm, "").trim())
    .find((part) => part && !part.startsWith("```") && !part.startsWith("<!--"))
    ?.slice(0, 500) ?? "";
}
