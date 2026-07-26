import { FormEvent, useEffect, useMemo, useRef, useState } from "react";
import { Link } from "react-router-dom";
import type { TrainingCategory, UserTrainingModule, WorkspaceData } from "../../shared/types";
import { loadWorkspace, saveWorkspace } from "../../infrastructure/persistence/workspaceRepository";
import { catalog } from "../training-library/content";
import { categoryHasDescendant, createId, exportWorkspace, importWorkspace, mergeCategories, mergeModules } from "./workspace";

type Editor =
  | { kind: "category"; value?: TrainingCategory }
  | { kind: "module"; value?: UserTrainingModule }
  | { kind: "merge-category" }
  | { kind: "merge-module" }
  | null;

const EMPTY: WorkspaceData = { schemaVersion: 1, categories: [], modules: [] };

export default function WorkspaceManager() {
  const [data, setData] = useState<WorkspaceData>(EMPTY);
  const [view, setView] = useState<"active" | "trash">("active");
  const [editor, setEditor] = useState<Editor>(null);
  const [message, setMessage] = useState("");
  const importRef = useRef<HTMLInputElement>(null);
  useEffect(() => { void loadWorkspace().then(setData); }, []);

  const update = async (next: WorkspaceData, notice: string) => {
    try {
      await saveWorkspace(next);
      setData(next);
      setMessage(notice);
      setEditor(null);
    } catch {
      setMessage("保存失败，请导出数据后重试。");
    }
  };
  const categories = data.categories.filter((item) => item.trashed === (view === "trash"));
  const modules = data.modules.filter((item) => item.trashed === (view === "trash"));
  const categoryNames = useMemo(() => new Map(data.categories.map((item) => [item.id, item.name])), [data.categories]);

  const exportAction = () => {
    const link = document.createElement("a");
    link.href = URL.createObjectURL(new Blob([exportWorkspace(data)], { type: "application/json" }));
    link.download = `loop-workspace-${new Date().toISOString().slice(0, 10)}.json`;
    link.click();
    URL.revokeObjectURL(link.href);
  };
  const importAction = async (file: File) => {
    try {
      const next = importWorkspace(await file.text());
      await update(next, `导入完成：${next.categories.length} 个分类，${next.modules.length} 个模块`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "导入失败");
    }
  };

  return (
    <section className="panel library-panel">
      <div className="library-heading"><div><div className="eyebrow">My Training</div><h1>我的训练</h1><p>分类和模块只表达你的训练组织方式，不改变知识库结构。</p></div><Link className="button-link secondary" to="/library">返回训练库</Link></div>
      <div className="manager-toolbar">
        <button className="secondary" onClick={() => setView(view === "active" ? "trash" : "active")}>{view === "active" ? "查看回收站" : "返回有效项目"}</button>
        <button className="secondary" onClick={exportAction}>导出工作区</button>
        <button className="secondary" onClick={() => importRef.current?.click()}>导入工作区</button>
        <input ref={importRef} hidden type="file" accept="application/json" onChange={(event) => { const file = event.target.files?.[0]; if (file) void importAction(file); event.target.value = ""; }} />
        {message && <span className="manager-message" role="status">{message}</span>}
      </div>
      <div className="manager-grid">
        <section className="manager-column">
          <div className="manager-heading"><div><h2>训练分类</h2><p>支持上下级分类、修改、回收和合并。</p></div>{view === "active" && <button className="secondary" onClick={() => setEditor({ kind: "category" })}>新增分类</button>}</div>
          {categories.map((item) => <article className="manager-card" key={item.id}><div><strong>{item.name}</strong><small>{item.parentId ? `上级：${categoryNames.get(item.parentId) ?? "已移除"}` : "顶级分类"}</small><p>{item.description}</p></div><div className="mini-actions">{view === "active" && <button onClick={() => setEditor({ kind: "category", value: item })}>修改</button>}<button onClick={() => void update({ ...data, categories: data.categories.map((entry) => entry.id === item.id ? { ...entry, trashed: view === "active" } : entry) }, view === "active" ? "分类已移入回收站" : "分类已恢复")}>{view === "active" ? "删除" : "恢复"}</button></div></article>)}
          {!categories.length && <div className="empty-state">当前没有分类。</div>}
          {view === "active" && data.categories.filter((item) => !item.trashed).length > 1 && <button className="text-action" onClick={() => setEditor({ kind: "merge-category" })}>合并分类</button>}
        </section>
        <section className="manager-column">
          <div className="manager-heading"><div><h2>训练模块</h2><p>组合多个训练单元，形成自己的训练入口。</p></div>{view === "active" && <button className="secondary" onClick={() => setEditor({ kind: "module" })}>新增模块</button>}</div>
          {modules.map((item) => <article className="manager-card" key={item.id}><div><strong>{item.name}</strong><small>{item.categoryIds.map((id) => categoryNames.get(id)).filter(Boolean).join(" / ") || "未分类"}</small><p>{item.unitIds.length} 个训练单元 · {item.description}</p></div><div className="mini-actions">{view === "active" && <button onClick={() => setEditor({ kind: "module", value: item })}>修改</button>}{view === "active" && item.unitIds[0] && <Link to={`/library/units/${item.unitIds[0]}`}>训练</Link>}<button onClick={() => void update({ ...data, modules: data.modules.map((entry) => entry.id === item.id ? { ...entry, trashed: view === "active" } : entry) }, view === "active" ? "模块已移入回收站" : "模块已恢复")}>{view === "active" ? "删除" : "恢复"}</button></div></article>)}
          {!modules.length && <div className="empty-state">当前没有训练模块。</div>}
          {view === "active" && data.modules.filter((item) => !item.trashed).length > 1 && <button className="text-action" onClick={() => setEditor({ kind: "merge-module" })}>合并模块</button>}
        </section>
      </div>
      {editor?.kind === "category" && <CategoryForm data={data} current={editor.value} onCancel={() => setEditor(null)} onSubmit={(next) => void update(next, editor.value ? "分类已修改" : "分类已创建")} />}
      {editor?.kind === "module" && <ModuleForm data={data} current={editor.value} onCancel={() => setEditor(null)} onSubmit={(next) => void update(next, editor.value ? "模块已修改" : "模块已创建")} />}
      {(editor?.kind === "merge-category" || editor?.kind === "merge-module") && <MergeForm kind={editor.kind} data={data} onCancel={() => setEditor(null)} onSubmit={(next) => void update(next, editor.kind === "merge-category" ? "分类已合并" : "模块已合并")} />}
    </section>
  );
}

function CategoryForm({ data, current, onCancel, onSubmit }: { data: WorkspaceData; current?: TrainingCategory; onCancel: () => void; onSubmit: (data: WorkspaceData) => void }) {
  const [name, setName] = useState(current?.name ?? "");
  const [description, setDescription] = useState(current?.description ?? "");
  const [parentId, setParentId] = useState(current?.parentId ?? "");
  const submit = (event: FormEvent) => {
    event.preventDefault();
    const value: TrainingCategory = current ? { ...current, name: name.trim(), description, parentId: parentId || null } : { id: createId("category"), name: name.trim(), description, parentId: parentId || null, trashed: false };
    onSubmit({ ...data, categories: current ? data.categories.map((item) => item.id === current.id ? value : item) : [...data.categories, value] });
  };
  const parentOptions = data.categories.filter((item) => !item.trashed && item.id !== current?.id && (!current || !categoryHasDescendant(data.categories, current.id, item.id)));
  return <form className="editor-card" onSubmit={submit}><div className="editor-heading"><h2>{current ? "修改分类" : "新增分类"}</h2><button type="button" onClick={onCancel}>关闭</button></div><label>分类名称<input required value={name} onChange={(event) => setName(event.target.value)} /></label><label>上级分类<select value={parentId} onChange={(event) => setParentId(event.target.value)}><option value="">顶级分类</option>{parentOptions.map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}</select></label><label>说明<textarea value={description} onChange={(event) => setDescription(event.target.value)} /></label><button className="primary" type="submit">保存分类</button></form>;
}

function ModuleForm({ data, current, onCancel, onSubmit }: { data: WorkspaceData; current?: UserTrainingModule; onCancel: () => void; onSubmit: (data: WorkspaceData) => void }) {
  const [name, setName] = useState(current?.name ?? "");
  const [description, setDescription] = useState(current?.description ?? "");
  const [unitIds, setUnitIds] = useState(current?.unitIds ?? []);
  const [categoryIds, setCategoryIds] = useState(current?.categoryIds ?? []);
  const toggle = (values: string[], value: string) => values.includes(value) ? values.filter((item) => item !== value) : [...values, value];
  const submit = (event: FormEvent) => {
    event.preventDefault();
    if (!unitIds.length) return;
    const now = new Date().toISOString();
    const value: UserTrainingModule = current ? { ...current, name: name.trim(), description, unitIds, categoryIds, updatedAt: now } : { id: createId("module"), name: name.trim(), description, unitIds, categoryIds, trashed: false, createdAt: now, updatedAt: now };
    onSubmit({ ...data, modules: current ? data.modules.map((item) => item.id === current.id ? value : item) : [...data.modules, value] });
  };
  return <form className="editor-card" onSubmit={submit}><div className="editor-heading"><h2>{current ? "修改模块" : "新增模块"}</h2><button type="button" onClick={onCancel}>关闭</button></div><label>模块名称<input required value={name} onChange={(event) => setName(event.target.value)} /></label><label>说明<textarea value={description} onChange={(event) => setDescription(event.target.value)} /></label><fieldset><legend>训练单元（至少选择一个）</legend>{catalog.units.filter((item) => item.status === "available").map((item) => <label className="check-option" key={item.id}><input type="checkbox" checked={unitIds.includes(item.id)} onChange={() => setUnitIds(toggle(unitIds, item.id))} />{item.title}</label>)}</fieldset><fieldset><legend>所属分类</legend>{data.categories.filter((item) => !item.trashed).map((item) => <label className="check-option" key={item.id}><input type="checkbox" checked={categoryIds.includes(item.id)} onChange={() => setCategoryIds(toggle(categoryIds, item.id))} />{item.name}</label>)}</fieldset><button className="primary" type="submit" disabled={!unitIds.length}>保存模块</button></form>;
}

function MergeForm({ kind, data, onCancel, onSubmit }: { kind: "merge-category" | "merge-module"; data: WorkspaceData; onCancel: () => void; onSubmit: (data: WorkspaceData) => void }) {
  const items = kind === "merge-category" ? data.categories.filter((item) => !item.trashed) : data.modules.filter((item) => !item.trashed);
  const [source, setSource] = useState(items[0]?.id ?? "");
  const [target, setTarget] = useState(items[1]?.id ?? "");
  const [error, setError] = useState("");
  const submit = (event: FormEvent) => {
    event.preventDefault();
    try { onSubmit(kind === "merge-category" ? mergeCategories(data, source, target) : mergeModules(data, source, target)); }
    catch (reason) { setError(reason instanceof Error ? reason.message : "合并失败"); }
  };
  return <form className="editor-card" onSubmit={submit}><div className="editor-heading"><h2>{kind === "merge-category" ? "合并分类" : "合并模块"}</h2><button type="button" onClick={onCancel}>关闭</button></div><p>源项目会被移除，目标项目保留；关联关系会自动去重。</p><label>源项目<select value={source} onChange={(event) => setSource(event.target.value)}>{items.map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}</select></label><label>目标项目<select value={target} onChange={(event) => setTarget(event.target.value)}>{items.map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}</select></label>{error && <p className="form-error">{error}</p>}<button className="primary" type="submit">确认合并</button></form>;
}
