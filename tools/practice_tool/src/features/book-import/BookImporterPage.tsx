import { ChangeEvent, useEffect, useRef, useState } from "react";
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
    let active = true;
    void loadImportedBook(editingId).then((book) => {
      if (!active) return;
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
    return () => { active = false; };
  }, [editingId]);

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

  const selectedChapters = chapters.filter((chapter) => chapter.confirmed);
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
    const validatedChapters = publish ? selectedChapters : chapters;
    if (!title.trim()) problems.push("缺少书名");
    if (!/^\d+\.\d+\.\d+$/.test(version)) problems.push("版本必须使用 x.y.z");
    if (!sourceId.trim()) problems.push("缺少知识源 ID");
    if (!chapters.length) problems.push("至少选择一个 Markdown 文件");
    if (publish && !selectedChapters.length) problems.push("至少勾选一个要发布的 Markdown 文件");
    if (validatedChapters.some((chapter) => !chapter.title.trim() || !chapter.markdown.trim())) problems.push("所选文件的章节标题或正文为空");
    if (mode === "topic" && validatedChapters.some((chapter) => !chapter.objective.trim() || !chapter.candidateClaim.trim() || !chapter.trainingPrompt.trim())) {
      problems.push("专题章节必须填写目标、候选声明和训练任务");
    }
    return problems;
  };

  const save = async (publish: boolean) => {
    const problems = validate(publish);
    if (problems.length) return setMessage(`无法${publish ? "发布" : "保存"}：${problems.join("；")}`);
    const now = new Date().toISOString();
    const savedChapters = publish ? selectedChapters : chapters;
    const book: ImportedBook = {
      schemaVersion: 1,
      id: bookId || makeId(),
      title: title.trim(),
      version,
      mode,
      status: publish ? "published" : "draft",
      sourceId: sourceId.trim(),
      outlineMarkdown: createOutline(title.trim(), mode, savedChapters),
      chapters: savedChapters,
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
  const cancelEditing = () => {
    setBookId("");
    setCreatedAt("");
    setMode("source");
    setTitle("");
    setVersion("0.1.0");
    setSourceId("local-import");
    setChapters([]);
    setSelected(0);
    setMessage("");
    navigate("/library/import", { replace: true });
  };
  const modePicker = <div className="mode-picker" role="group" aria-label="导入模式">
    <button className={`has-tooltip ${mode === "source" ? "selected" : ""}`} data-tooltip="原样保存正文，只生成书籍与章节目录" aria-label="原文阅读：原样保存正文，只生成书籍与章节目录" onClick={() => changeMode("source")}><strong>原文阅读</strong></button>
    <button className={`has-tooltip ${mode === "topic" ? "selected" : ""}`} data-tooltip="生成目标、候选声明和训练任务草稿" aria-label="专题电子书：生成目标、候选声明和训练任务草稿" onClick={() => changeMode("topic")}><strong>专题电子书</strong></button>
  </div>;
  const bookSettings = <div className="book-settings">
    <label><span>书名</span><input value={title} onChange={(event) => setTitle(event.target.value)} /></label>
    <label><span>版本</span><input value={version} onChange={(event) => setVersion(event.target.value)} /></label>
    <label><span>知识源</span><select value={sourceId} onChange={(event) => setSourceId(event.target.value)}><option value="local-import">本机临时导入</option>{runtimeConfig.sources.map((source) => <option value={source.id} key={source.id}>{source.title}（{source.id}）</option>)}</select></label>
  </div>;

  return <section className={`book-import-page editing ${chapters.length > 0 ? `${mode}-mode has-content` : "idle"}`}>
    <header className="import-heading">
      <div><div className="eyebrow">Book Importer</div><div className="heading-with-hint"><h1>电子书导入器</h1><span className="info-hint" tabIndex={0} role="img" aria-label="原文阅读保留原始正文；专题电子书额外生成候选声明和训练任务草稿，并在发布前逐章确认。" data-tooltip="原文阅读保留原始正文；专题电子书额外生成候选声明和训练任务草稿，并在发布前逐章确认。">i</span></div></div>
      {modePicker}
      {bookSettings}
      <div className="import-file-actions">
        <label className="file-picker secondary-picker">选择 Markdown 文件<input type="file" accept=".md,text/markdown" multiple onChange={(event) => void importFiles(event)} /></label>
        <label className="file-picker">选择整个目录<input ref={directoryInput} type="file" accept=".md,text/markdown" multiple onChange={(event) => void importFiles(event)} /></label>
      </div>
    </header>
    <div className="import-workspace">
      <aside className={`import-chapters ${chapters.length > 0 ? "has-action-dock" : ""}`}>
        <div className="chapter-toolbar">
          {chapters.length > 0 ? <label className="confirm-all" title="全选或取消全部候选文件">
            <input ref={confirmAllInput} type="checkbox" checked={allConfirmed} onChange={(event) => setAllConfirmed(event.target.checked)} />
            <small role="status" aria-label={`已选择 ${confirmedCount}/${chapters.length} 个文件`}>{confirmedCount}/{chapters.length}</small>
          </label> : <span className="confirm-column-placeholder" />}
          <span className="chapter-toolbar-title"><strong>目录</strong></span>
          <span className="chapter-order-actions"><button onClick={() => move(-1)} disabled={selected === 0}>↑</button><button onClick={() => move(1)} disabled={selected >= chapters.length - 1}>↓</button></span>
        </div>
        <div className="import-chapter-list">
          {chapters.map((chapter, index) => <div className={`import-chapter-row ${selected === index ? "active" : ""}`} key={chapter.id}>
            <label className="chapter-confirm" title={chapter.confirmed ? "已纳入发布内容" : "勾选以纳入发布内容"}><input type="checkbox" checked={chapter.confirmed} onChange={(event) => setChapterConfirmed(index, event.target.checked)} /></label>
            <button className="has-tooltip" data-tooltip={`完整路径：${chapter.sourceName}；正文标题：${chapter.title}`} aria-label={`${fileNameOf(chapter.sourceName)}，完整路径：${chapter.sourceName}，正文标题：${chapter.title}`} onClick={() => setSelected(index)}><b>{String(index + 1).padStart(2, "0")}</b><span>{fileNameOf(chapter.sourceName)}</span></button>
          </div>)}
          {!chapters.length && <p>请选择一个或多个 Markdown 文件。</p>}
        </div>
        {chapters.length > 0 && <div className="import-action-bar">
          <div className={editingId ? "editing-actions" : ""}>
            {editingId && <button className="cancel-edit" onClick={cancelEditing}>取消编辑</button>}
            <button className="secondary" onClick={() => void save(false)}>保存草稿</button>
            <button className="primary" onClick={() => void save(true)}>发布</button>
          </div>
        </div>}
      </aside>
      <main className="import-editor">
        {chapters[selected] ? <>
          <div className="import-editor-toolbar">
            <strong className="preview-heading">▼ 全文预览</strong>
            <span className="current-source has-tooltip" data-tooltip={`当前预览源文件：${chapters[selected].sourceName}`}>{chapters[selected].sourceName}</span>
            <label className="chapter-title-inline" data-tooltip="可直接修改目录中显示的章节标题"><span>章节标题</span><input value={chapters[selected].title} aria-label="章节标题" onChange={(event) => updateChapter({ title: event.target.value })} /></label>
          </div>
          {mode === "topic" && <>
            <label>本章目标<textarea value={chapters[selected].objective} onChange={(event) => updateChapter({ objective: event.target.value })} /></label>
            <label>候选知识声明<textarea value={chapters[selected].candidateClaim} onChange={(event) => updateChapter({ candidateClaim: event.target.value })} /></label>
            <label>训练任务草稿<textarea value={chapters[selected].trainingPrompt} onChange={(event) => updateChapter({ trainingPrompt: event.target.value })} /></label>
          </>}
          <div className="markdown-preview"><MarkdownGuide markdown={chapters[selected].markdown} /></div>
        </> : <div className="empty-state">导入 Markdown 后在这里编辑并预览。</div>}
      </main>
    </div>
    {books.length > 0 && chapters.length === 0 && <section className="local-book-list"><h2>本机电子书</h2>{books.map((book) => <article key={book.id}>
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

function fileNameOf(sourceName: string) {
  return sourceName.replaceAll("\\", "/").split("/").pop() || sourceName;
}

function extractFirstParagraph(markdown: string) {
  return markdown
    .replace(/^---[\s\S]*?---\s*/m, "")
    .split(/\n\s*\n/)
    .map((part) => part.replace(/^#+\s+.*$/gm, "").trim())
    .find((part) => part && !part.startsWith("```") && !part.startsWith("<!--"))
    ?.slice(0, 500) ?? "";
}

function createOutline(title: string, mode: ImportedBookMode, chapters: ImportedBookChapter[]) {
  return [
    `# ${title || "未命名电子书"}`,
    "",
    mode === "source" ? "本书按所选原始 Markdown 顺序编排。" : "本书是由权威材料生成的专题提炼草稿，候选结论与训练任务需人工确认。",
    "",
    ...chapters.map((chapter, index) => `${index + 1}. ${chapter.title}`),
  ].join("\n");
}
