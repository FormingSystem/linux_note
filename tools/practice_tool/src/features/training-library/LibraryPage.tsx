import { useEffect, useMemo, useRef, useState } from "react";
import { Link } from "react-router-dom";
import type { ImportedBook, TrainingCategory, WorkspaceData } from "../../shared/types";
import { loadWorkspace, saveWorkspace } from "../../infrastructure/persistence/workspaceRepository";
import { listImportedBooks } from "../../infrastructure/persistence/importedBookRepository";
import { CategoryForm } from "../training-management/WorkspaceManager";
import { categoryHasDescendant } from "../training-management/workspace";
import { catalog } from "./content";

const EMPTY_WORKSPACE: WorkspaceData = { schemaVersion: 1, categories: [], modules: [] };

export default function LibraryPage() {
  const [query, setQuery] = useState("");
  const [scope, setScope] = useState("all");
  const [workspace, setWorkspace] = useState<WorkspaceData>(EMPTY_WORKSPACE);
  const [editor, setEditor] = useState<{ value?: TrainingCategory; parentId?: string } | null>(null);
  const [configTarget, setConfigTarget] = useState<{ value: TrainingCategory; anchor: { top: number; left: number } } | null>(null);
  const [dropTarget, setDropTarget] = useState<string | null | undefined>(undefined);
  const [draggedUnitId, setDraggedUnitId] = useState<string>();
  const [selectedUnitId, setSelectedUnitId] = useState<string>();
  const [message, setMessage] = useState("");
  const [past, setPast] = useState<WorkspaceData[]>([]);
  const [future, setFuture] = useState<WorkspaceData[]>([]);
  const [importedBooks, setImportedBooks] = useState<ImportedBook[]>([]);

  useEffect(() => {
    void listImportedBooks().then((items) => setImportedBooks(items.filter((item) => item.status === "published")));
  }, []);

  useEffect(() => {
    void loadWorkspace().then(async (stored) => {
      if (stored.categoryTreeInitialized) return setWorkspace(stored);
      const initialized = { ...stored, categories: stored.categories.length ? stored.categories : createDefaultDirectories(), categoryTreeInitialized: true };
      await saveWorkspace(initialized);
      setWorkspace(initialized);
    });
  }, []);

  const categories = workspace.categories.filter((item) => !item.trashed).sort((a, b) => a.sortOrder - b.sortOrder);
  const units = useMemo(() => {
    const normalized = query.trim().toLowerCase();
    const allowedIds = scope === "all" ? null : collectUnitIds(categories, scope.slice(9));
    return catalog.units.filter((item) => {
      const haystack = [item.title, item.summary, item.module, ...item.tags].join(" ").toLowerCase();
      return item.status === "available" && (!allowedIds || allowedIds.has(item.id)) && (!normalized || haystack.includes(normalized));
    });
  }, [categories, query, scope]);
  const selectedTitle = scope === "all" ? "全部单元" : categories.find((item) => item.id === scope.slice(9))?.name ?? "单元目录";
  const historyLimit = workspace.historyLimit ?? 1000;

  const updateWorkspace = async (next: WorkspaceData, notice: string) => {
    await saveWorkspace(next);
    setPast((items) => [...(historyLimit > 1 ? items.slice(-(historyLimit - 1)) : []), workspace]);
    setFuture([]);
    setWorkspace(next);
    setMessage(notice);
    setEditor(null);
    setConfigTarget(null);
  };
  const trashCategory = (target: TrainingCategory) => {
    void updateWorkspace({
      ...workspace,
      categories: workspace.categories.map((item) => item.id === target.id
        ? { ...item, trashed: true }
        : item.parentId === target.id ? { ...item, parentId: target.parentId } : item),
      modules: workspace.modules.map((item) => ({ ...item, categoryIds: item.categoryIds.filter((id) => id !== target.id) })),
    }, "目录已移入回收站，子目录已提升一级");
    if (scope === `category:${target.id}`) setScope("all");
  };
  const moveDirectory = (sourceId: string, parentId: string | null) => {
    if (sourceId === parentId || (parentId && categoryHasDescendant(categories, sourceId, parentId))) {
      setMessage("不能把目录移动到自身或其子目录中");
      return;
    }
    const nextOrder = Math.max(-1, ...categories.filter((item) => item.parentId === parentId).map((item) => item.sortOrder)) + 1;
    void updateWorkspace({ ...workspace, categories: workspace.categories.map((item) => item.id === sourceId ? { ...item, parentId, sortOrder: nextOrder } : item) }, parentId ? "目录已移动" : "目录已提升为顶级");
  };
  const reorderDirectory = (sourceId: string, targetId: string, position: "before" | "after") => {
    const source = categories.find((item) => item.id === sourceId);
    const target = categories.find((item) => item.id === targetId);
    if (!source || !target || sourceId === targetId) return;
    const siblings = categories.filter((item) => item.parentId === target.parentId && item.id !== sourceId);
    const targetIndex = siblings.findIndex((item) => item.id === targetId);
    siblings.splice(targetIndex + (position === "after" ? 1 : 0), 0, source);
    const orders = new Map(siblings.map((item, index) => [item.id, index]));
    void updateWorkspace({ ...workspace, categories: workspace.categories.map((item) => orders.has(item.id) ? { ...item, parentId: target.parentId, sortOrder: orders.get(item.id)! } : item) }, "目录顺序已调整");
  };
  const stepDirectory = (sourceId: string, direction: -1 | 1) => {
    const source = categories.find((item) => item.id === sourceId);
    if (!source) return;
    const siblings = categories.filter((item) => item.parentId === source.parentId);
    const index = siblings.findIndex((item) => item.id === sourceId);
    const target = siblings[index + direction];
    if (target) reorderDirectory(sourceId, target.id, direction < 0 ? "before" : "after");
  };
  const moveUnit = (unitId: string, targetCategoryId: string) => {
    const exclusive = (workspace.unitAssignmentMode ?? "exclusive") === "exclusive";
    const nextCategories = workspace.categories.map((category) => {
      const currentUnitIds = category.unitIds ?? [];
      if (category.id === targetCategoryId) return { ...category, unitIds: [...new Set([...currentUnitIds, unitId])] };
      return exclusive ? { ...category, unitIds: currentUnitIds.filter((id) => id !== unitId) } : category;
    });
    void updateWorkspace({ ...workspace, categories: nextCategories }, exclusive ? "训练单元已移动" : "训练单元已加入目录");
  };
  const reorderUnit = (categoryId: string, unitId: string, direction: -1 | 1) => {
    const category = workspace.categories.find((item) => item.id === categoryId);
    if (!category) return;
    const unitIds = [...(category.unitIds ?? [])];
    const index = unitIds.indexOf(unitId);
    const targetIndex = index + direction;
    if (index < 0 || targetIndex < 0 || targetIndex >= unitIds.length) return;
    [unitIds[index], unitIds[targetIndex]] = [unitIds[targetIndex], unitIds[index]];
    void updateWorkspace({ ...workspace, categories: workspace.categories.map((item) => item.id === categoryId ? { ...item, unitIds } : item) }, "训练单元顺序已调整");
  };
  const placeUnit = (unitId: string, sourceCategoryId: string, targetCategoryId: string, targetUnitId: string, position: "before" | "after") => {
    if (unitId === targetUnitId && sourceCategoryId === targetCategoryId) return;
    const exclusive = (workspace.unitAssignmentMode ?? "exclusive") === "exclusive";
    const target = workspace.categories.find((item) => item.id === targetCategoryId);
    if (!target) return;
    const targetUnitIds = (target.unitIds ?? []).filter((id) => id !== unitId);
    const targetIndex = targetUnitIds.indexOf(targetUnitId);
    targetUnitIds.splice(targetIndex + (position === "after" ? 1 : 0), 0, unitId);
    const categories = workspace.categories.map((category) => {
      if (category.id === targetCategoryId) return { ...category, unitIds: targetUnitIds };
      if (exclusive || category.id === sourceCategoryId) return { ...category, unitIds: (category.unitIds ?? []).filter((id) => id !== unitId) };
      return category;
    });
    void updateWorkspace({ ...workspace, categories }, sourceCategoryId === targetCategoryId ? "训练单元顺序已调整" : exclusive ? "训练单元已移动并排序" : "训练单元已加入目录并排序");
  };
  const selectUnitFromTree = (categoryId: string, unitId: string) => {
    setScope(`category:${categoryId}`);
    setSelectedUnitId(unitId);
    window.requestAnimationFrame(() => document.getElementById(`unit-card-${unitId}`)?.scrollIntoView({ behavior: "smooth", block: "center" }));
  };
  const undo = async () => {
    const previous = past.at(-1);
    if (!previous) return;
    await saveWorkspace(previous);
    setPast((items) => items.slice(0, -1));
    setFuture((items) => [workspace, ...items].slice(0, historyLimit));
    setWorkspace(previous);
    setMessage("已撤销上一步操作");
    setEditor(null);
    setConfigTarget(null);
  };
  const redo = async () => {
    const next = future[0];
    if (!next) return;
    await saveWorkspace(next);
    setFuture((items) => items.slice(1));
    setPast((items) => [...(historyLimit > 1 ? items.slice(-(historyLimit - 1)) : []), workspace]);
    setWorkspace(next);
    setMessage("已重做下一步操作");
    setEditor(null);
    setConfigTarget(null);
  };

  useEffect(() => {
    window.dispatchEvent(new CustomEvent("loop-history-state", { detail: { canUndo: past.length > 0, canRedo: future.length > 0 } }));
  }, [future.length, past.length]);
  useEffect(() => () => {
    window.dispatchEvent(new CustomEvent("loop-history-state", { detail: { canUndo: false, canRedo: false } }));
  }, []);
  useEffect(() => {
    const handleUndo = () => void undo();
    const handleRedo = () => void redo();
    window.addEventListener("loop-history-undo", handleUndo);
    window.addEventListener("loop-history-redo", handleRedo);
    return () => {
      window.removeEventListener("loop-history-undo", handleUndo);
      window.removeEventListener("loop-history-redo", handleRedo);
    };
  });

  useEffect(() => {
    const handleHistoryShortcut = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement | null;
      if (target?.closest("input, textarea, select, [contenteditable='true']") || !(event.ctrlKey || event.metaKey)) return;
      if (event.key.toLowerCase() === "z" && event.shiftKey) {
        event.preventDefault();
        void redo();
      } else if (event.key.toLowerCase() === "z") {
        event.preventDefault();
        void undo();
      } else if (event.key.toLowerCase() === "y") {
        event.preventDefault();
        void redo();
      }
    };
    document.addEventListener("keydown", handleHistoryShortcut);
    return () => document.removeEventListener("keydown", handleHistoryShortcut);
  });

  return (
    <section className="panel library-panel">
      <div className="library-heading">
        <div><div className="eyebrow">Training Library</div><h1>训练库</h1><p>按自己的目录组织训练单元，再开始或继续训练。</p></div>
        <Link className="button-link primary" to="/library/import">导入电子书</Link>
      </div>
      {importedBooks.length > 0 && <section className="published-book-strip"><div><strong>我的电子书</strong><small>{importedBooks.length} 本已发布</small></div>{importedBooks.map((book) => <Link key={book.id} to={`/library/books/${book.id}/${book.chapters[0]?.id ?? ""}`}><span>{book.mode === "source" ? "原文阅读" : "专题电子书"}</span><strong>{book.title}</strong><small>{book.chapters.length} 章 · v{book.version}</small></Link>)}</section>}
      <div className="library-workspace">
        <aside className="library-directory" aria-label="训练单元目录">
          <div className="library-search">
            <span aria-hidden="true">⌕</span>
            <input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="搜索训练内容…" aria-label="搜索训练单元" />
            {query && <button type="button" onClick={() => setQuery("")} aria-label="清除搜索">×</button>}
          </div>
          <div className="directory-title">
            <div><span>单元目录</span><small>分类、移动与整理</small></div>
            <button type="button" onClick={() => setEditor({})}>＋ 目录</button>
          </div>
          <div className="outline-tree">
            <div
              className={`outline-node-row root-node ${scope === "all" ? "active" : ""} ${dropTarget === null ? "drop-target" : ""}`}
              onDragOver={(event) => { event.preventDefault(); setDropTarget(null); }}
              onDragLeave={() => setDropTarget(undefined)}
              onDrop={(event) => { event.preventDefault(); const sourceId = event.dataTransfer.getData("text/directory-id"); if (sourceId) moveDirectory(sourceId, null); setDropTarget(undefined); }}
            >
              <button className="outline-node-name" onClick={() => setScope("all")}>全部单元</button>
              <b>{catalog.units.length}</b>
            </div>
            {categories.filter((item) => !item.parentId).map((category) => (
              <DirectoryNode key={category.id} category={category} categories={categories} scope={scope} selectedUnitId={selectedUnitId} dropTarget={dropTarget} onDropTarget={setDropTarget} onMove={moveDirectory} onMoveUnit={moveUnit} onPlaceUnit={placeUnit} onReorder={reorderDirectory} onReorderUnit={reorderUnit} onStep={stepDirectory} onSelect={setScope} onSelectUnit={selectUnitFromTree} onConfigure={(value, anchor) => setConfigTarget({ value, anchor })} />
            ))}
            {!categories.length && <p className="outline-empty">目录为空</p>}
            {message && <small className="outline-message" role="status">{message}</small>}
          </div>
        </aside>
        <div className="library-results">
          <div className="results-heading"><div><span>当前目录</span><strong>{selectedTitle}</strong></div><small>{units.length} 个训练单元</small></div>
          <div className="unit-list">
            {units.map((item) => (
              <Link
                id={`unit-card-${item.id}`}
                className={`unit-card ${draggedUnitId === item.id ? "dragging" : ""} ${selectedUnitId === item.id ? "tree-selected" : ""}`}
                key={item.id}
                to={`/library/units/${item.id}`}
                aria-label={`查看训练单元：${item.title}`}
                draggable
                onDragStart={(event) => {
                  event.dataTransfer.effectAllowed = "move";
                  event.dataTransfer.setData("text/unit-id", item.id);
                  setDraggedUnitId(item.id);
                }}
                onDragEnd={() => {
                  setDraggedUnitId(undefined);
                  setDropTarget(undefined);
                }}
              >
                <div className="unit-card-top"><span>{item.domain} / {item.topic} / {item.module}</span><span className="unit-card-controls"><i aria-hidden="true">⠿</i><small>拖动归类</small><b>可训练</b></span></div>
                <h2>{item.title}</h2><p>{item.summary}</p>
                <div className="unit-tags">{item.tags.map((tag) => <span key={tag}>{tag}</span>)}</div>
                <div className="unit-card-footer"><small>{item.estimated_minutes} 分钟 · {item.level}</small><span className="unit-card-cta">查看详情 <i aria-hidden="true">→</i></span></div>
              </Link>
            ))}
            {!units.length && <div className="empty-state">当前目录没有匹配的训练单元。</div>}
          </div>
        </div>
      </div>
      {editor && <CategoryForm data={workspace} current={editor.value} initialParentId={editor.parentId} onCancel={() => setEditor(null)} onSubmit={(next) => void updateWorkspace(next, editor.value ? "目录已更新" : "目录已创建")} />}
      {configTarget && <DirectoryConfigCard
        target={configTarget.value}
        anchor={configTarget.anchor}
        onClose={() => setConfigTarget(null)}
        onEdit={() => { setEditor({ value: configTarget.value }); setConfigTarget(null); }}
        onAddChild={() => { setEditor({ parentId: configTarget.value.id }); setConfigTarget(null); }}
        onTrash={() => trashCategory(configTarget.value)}
      />}
    </section>
  );
}

function DirectoryNode({ category, categories, scope, selectedUnitId, dropTarget, onDropTarget, onMove, onMoveUnit, onPlaceUnit, onReorder, onReorderUnit, onStep, onSelect, onSelectUnit, onConfigure }: { category: TrainingCategory; categories: TrainingCategory[]; scope: string; selectedUnitId?: string; dropTarget: string | null | undefined; onDropTarget: (id: string | null | undefined) => void; onMove: (sourceId: string, parentId: string | null) => void; onMoveUnit: (unitId: string, categoryId: string) => void; onPlaceUnit: (unitId: string, sourceCategoryId: string, targetCategoryId: string, targetUnitId: string, position: "before" | "after") => void; onReorder: (sourceId: string, targetId: string, position: "before" | "after") => void; onReorderUnit: (categoryId: string, unitId: string, direction: -1 | 1) => void; onStep: (sourceId: string, direction: -1 | 1) => void; onSelect: (scope: string) => void; onSelectUnit: (categoryId: string, unitId: string) => void; onConfigure: (category: TrainingCategory, anchor: { top: number; left: number }) => void }) {
  const children = categories.filter((item) => item.parentId === category.id);
  const directUnits = (category.unitIds ?? []).map((unitId) => catalog.units.find((item) => item.id === unitId)).filter((item): item is NonNullable<typeof item> => Boolean(item));
  const [expanded, setExpanded] = useState(true);
  return (
    <div className="outline-node">
      <div className="directory-drop-line" onDragOver={(event) => event.preventDefault()} onDrop={(event) => { event.preventDefault(); event.stopPropagation(); const sourceId = event.dataTransfer.getData("text/directory-id"); if (sourceId) onReorder(sourceId, category.id, "before"); }} />
      <div
        className={`outline-node-row ${scope === `category:${category.id}` ? "active" : ""} ${dropTarget === category.id ? "drop-target" : ""}`}
        draggable
        onDragStart={(event) => { event.dataTransfer.effectAllowed = "move"; event.dataTransfer.setData("text/directory-id", category.id); }}
        onDragEnd={() => onDropTarget(undefined)}
        onDragOver={(event) => { event.preventDefault(); event.stopPropagation(); event.dataTransfer.dropEffect = "move"; onDropTarget(category.id); }}
        onDragLeave={(event) => { if (!event.currentTarget.contains(event.relatedTarget as Node | null)) onDropTarget(undefined); }}
        onDrop={(event) => { event.preventDefault(); event.stopPropagation(); const sourceId = event.dataTransfer.getData("text/directory-id"); const unitId = event.dataTransfer.getData("text/unit-id"); if (sourceId) onMove(sourceId, category.id); else if (unitId) onMoveUnit(unitId, category.id); onDropTarget(undefined); }}
        tabIndex={0}
        onKeyDown={(event) => {
          if (event.key === "ArrowUp") { event.preventDefault(); onStep(category.id, -1); }
          if (event.key === "ArrowDown") { event.preventDefault(); onStep(category.id, 1); }
        }}
      >
        {children.length > 0 || directUnits.length > 0
          ? <button className="outline-toggle" aria-label={expanded ? `收起${category.name}` : `展开${category.name}`} aria-expanded={expanded} onClick={(event) => { event.stopPropagation(); setExpanded((value) => !value); }}><ChevronIcon /></button>
          : <span className="outline-toggle-spacer" />}
        <button className="outline-node-name" onClick={() => onSelect(`category:${category.id}`)}>{category.name}</button>
        <b>{collectUnitIds(categories, category.id).size}</b>
        <div className="outline-node-actions"><button title="配置目录" aria-label={`配置${category.name}`} onClick={(event) => {
          const bounds = event.currentTarget.getBoundingClientRect();
          const width = 252;
          const height = 280;
          onConfigure(category, {
            left: Math.min(bounds.right + 8, window.innerWidth - width - 16),
            top: Math.max(72, Math.min(bounds.top - 8, window.innerHeight - height - 16)),
          });
        }}><SettingsIcon /></button></div>
      </div>
      {(children.length > 0 || directUnits.length > 0) && expanded && <div className="outline-node-children">
        {directUnits.map((unit) => <div className={`outline-unit-row ${selectedUnitId === unit.id ? "selected" : ""}`} key={unit.id} draggable
          onDragStart={(event) => { event.stopPropagation(); event.dataTransfer.effectAllowed = "move"; event.dataTransfer.setData("text/unit-id", unit.id); event.dataTransfer.setData("text/unit-category-id", category.id); }}
          onDragOver={(event) => { if (event.dataTransfer.types.includes("text/unit-id")) { event.preventDefault(); event.stopPropagation(); const positionClass = event.clientY < event.currentTarget.getBoundingClientRect().top + event.currentTarget.offsetHeight / 2 ? "drop-before" : "drop-after"; event.currentTarget.classList.remove("drop-before", "drop-after"); event.currentTarget.classList.add(positionClass); } }}
          onDragLeave={(event) => { event.currentTarget.classList.remove("drop-before", "drop-after"); }}
          onDrop={(event) => { event.preventDefault(); event.stopPropagation(); const sourceUnitId = event.dataTransfer.getData("text/unit-id"); const sourceCategoryId = event.dataTransfer.getData("text/unit-category-id"); const position = event.clientY < event.currentTarget.getBoundingClientRect().top + event.currentTarget.offsetHeight / 2 ? "before" : "after"; event.currentTarget.classList.remove("drop-before", "drop-after"); if (sourceUnitId) onPlaceUnit(sourceUnitId, sourceCategoryId, category.id, unit.id, position); }}
          onDragEnd={(event) => event.currentTarget.classList.remove("drop-before", "drop-after")}
          title="拖到其他单元前后可排序，拖到目录可调整归类"><span aria-hidden="true">◆</span><button type="button" aria-label={`${unit.title}；选中后可按上下方向键排序`} onClick={() => onSelectUnit(category.id, unit.id)} onKeyDown={(event) => { if (event.key === "ArrowUp") { event.preventDefault(); onReorderUnit(category.id, unit.id, -1); } if (event.key === "ArrowDown") { event.preventDefault(); onReorderUnit(category.id, unit.id, 1); } }}>{unit.title}</button></div>)}
        {children.map((child) => <DirectoryNode key={child.id} category={child} categories={categories} scope={scope} selectedUnitId={selectedUnitId} dropTarget={dropTarget} onDropTarget={onDropTarget} onMove={onMove} onMoveUnit={onMoveUnit} onPlaceUnit={onPlaceUnit} onReorder={onReorder} onReorderUnit={onReorderUnit} onStep={onStep} onSelect={onSelect} onSelectUnit={onSelectUnit} onConfigure={onConfigure} />)}
      </div>}
      <div className="directory-drop-line" onDragOver={(event) => event.preventDefault()} onDrop={(event) => { event.preventDefault(); event.stopPropagation(); const sourceId = event.dataTransfer.getData("text/directory-id"); if (sourceId) onReorder(sourceId, category.id, "after"); }} />
    </div>
  );
}

function SettingsIcon() {
  return (
    <svg className="settings-icon" viewBox="0 0 20 20" aria-hidden="true">
      <path d="M3 5h8M15 5h2M3 10h2M9 10h8M3 15h7M14 15h3" />
      <circle cx="13" cy="5" r="2" />
      <circle cx="7" cy="10" r="2" />
      <circle cx="12" cy="15" r="2" />
    </svg>
  );
}

function ChevronIcon() {
  return (
    <svg className="chevron-icon" viewBox="0 0 16 16" aria-hidden="true">
      <path d="m5.5 3.75 4.25 4.25-4.25 4.25" />
    </svg>
  );
}

function DirectoryConfigCard({ target, anchor, onClose, onEdit, onAddChild, onTrash }: { target: TrainingCategory; anchor: { top: number; left: number }; onClose: () => void; onEdit: () => void; onAddChild: () => void; onTrash: () => void }) {
  const cardRef = useRef<HTMLElement>(null);
  useEffect(() => {
    const closeFromOutside = (event: PointerEvent) => {
      if (!cardRef.current?.contains(event.target as Node)) onClose();
    };
    const closeFromKeyboard = (event: KeyboardEvent) => {
      if (event.key === "Escape") onClose();
    };
    document.addEventListener("pointerdown", closeFromOutside);
    document.addEventListener("keydown", closeFromKeyboard);
    return () => {
      document.removeEventListener("pointerdown", closeFromOutside);
      document.removeEventListener("keydown", closeFromKeyboard);
    };
  }, [onClose]);
  return (
    <section ref={cardRef} className="outline-config-card" style={{ top: anchor.top, left: anchor.left }} aria-label={`${target.name}目录配置`}>
      <div><span>目录配置</span><button onClick={onClose} aria-label="关闭配置">×</button></div>
      <strong>{target.name}</strong>
      <button onClick={onEdit}>编辑目录<i>→</i></button>
      <button onClick={onAddChild}>新建子目录<i>＋</i></button>
      <button className="danger-action" onClick={onTrash}>移入回收站<i>×</i></button>
      <Link to="/my-training">回收站与批量管理</Link>
    </section>
  );
}

function collectUnitIds(categories: TrainingCategory[], categoryId: string): Set<string> {
  const ids = new Set<string>();
  const visit = (id: string) => {
    const category = categories.find((item) => item.id === id);
    category?.unitIds.forEach((unitId) => ids.add(unitId));
    categories.filter((item) => item.parentId === id).forEach((child) => visit(child.id));
  };
  visit(categoryId);
  return ids;
}

function createDefaultDirectories(): TrainingCategory[] {
  const domains = [...new Set(catalog.units.map((item) => item.domain))];
  return domains.flatMap((domain, domainIndex) => {
    const domainId = `directory.${domain}`;
    const topics = [...new Set(catalog.units.filter((item) => item.domain === domain).map((item) => item.topic))];
    return [
      { id: domainId, name: domain.toUpperCase(), parentId: null, description: `${domain} 训练单元`, unitIds: [], sortOrder: domainIndex, trashed: false },
      ...topics.map((topic, index) => ({ id: `${domainId}.${topic}`, name: topic, parentId: domainId, description: `${topic} 训练单元`, unitIds: catalog.units.filter((item) => item.domain === domain && item.topic === topic).map((item) => item.id), sortOrder: index, trashed: false })),
    ];
  });
}
