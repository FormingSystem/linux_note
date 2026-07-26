import { useMemo, useRef, useState } from "react";
import type { UnitCatalogItem, WorkspaceData } from "../../shared/types";
import { loadWorkspace, saveWorkspace } from "../../infrastructure/persistence/workspaceStorage";
import {
  categoryHasDescendant,
  createId,
  exportWorkspace,
  importWorkspace,
  mergeCategories,
  mergeModules,
} from "./workspace";

type Props = {
  catalogUnits: UnitCatalogItem[];
  onTrain: (unit: UnitCatalogItem) => void;
};

export default function WorkspaceManager({ catalogUnits, onTrain }: Props) {
  const [data, setData] = useState<WorkspaceData>(loadWorkspace);
  const [view, setView] = useState<"active" | "trash">("active");
  const [message, setMessage] = useState("");
  const importRef = useRef<HTMLInputElement>(null);

  const update = (next: WorkspaceData, notice = "") => {
    saveWorkspace(next);
    setData(next);
    setMessage(notice);
  };

  const categories = data.categories.filter((item) => item.trashed === (view === "trash"));
  const modules = data.modules.filter((item) => item.trashed === (view === "trash"));
  const categoryNames = useMemo(() => new Map(data.categories.map((item) => [item.id, item.name])), [data.categories]);

  const addCategory = () => {
    const name = prompt("分类名称");
    if (!name?.trim()) return;
    const parentId = prompt("父分类 ID（顶级分类留空）")?.trim() || null;
    if (parentId && !data.categories.some((item) => item.id === parentId && !item.trashed)) {
      setMessage("父分类不存在");
      return;
    }
    update({
      ...data,
      categories: [...data.categories, {
        id: createId("category"),
        name: name.trim(),
        description: "",
        parentId,
        trashed: false,
      }],
    }, "分类已创建");
  };

  const editCategory = (id: string) => {
    const current = data.categories.find((item) => item.id === id);
    if (!current) return;
    const name = prompt("新的分类名称", current.name);
    if (!name?.trim()) return;
    const description = prompt("分类说明", current.description) ?? current.description;
    const parentId = prompt("父分类 ID（移到顶级请留空）", current.parentId || "")?.trim() || null;
    if (parentId === id || (parentId && (!data.categories.some((item) => item.id === parentId && !item.trashed) || categoryHasDescendant(data.categories, id, parentId)))) {
      setMessage("父分类无效");
      return;
    }
    update({ ...data, categories: data.categories.map((item) => item.id === id ? { ...item, name: name.trim(), description, parentId } : item) }, "分类已修改或移动");
  };

  const addModule = () => {
    const name = prompt("训练模块名称");
    if (!name?.trim()) return;
    const available = catalogUnits.map((item) => `${item.id}：${item.title}`).join("\n");
    const unitIds = (prompt(`输入训练单元 ID，多个用逗号分隔：\n${available}`) || "")
      .split(",").map((item) => item.trim()).filter((id) => catalogUnits.some((unit) => unit.id === id));
    if (!unitIds.length) {
      setMessage("至少选择一个有效训练单元");
      return;
    }
    const now = new Date().toISOString();
    update({
      ...data,
      modules: [...data.modules, {
        id: createId("module"),
        name: name.trim(),
        description: "",
        unitIds,
        categoryIds: [],
        trashed: false,
        createdAt: now,
        updatedAt: now,
      }],
    }, "训练模块已创建");
  };

  const editModule = (id: string) => {
    const current = data.modules.find((item) => item.id === id);
    if (!current) return;
    const name = prompt("训练模块名称", current.name);
    if (!name?.trim()) return;
    const description = prompt("训练模块说明", current.description) ?? current.description;
    const available = catalogUnits.map((item) => `${item.id}：${item.title}`).join("\n");
    const unitIds = (prompt(`训练单元 ID，多个用逗号分隔：\n${available}`, current.unitIds.join(",")) || "")
      .split(",").map((item) => item.trim()).filter((unitId) => catalogUnits.some((item) => item.id === unitId));
    if (!unitIds.length) {
      setMessage("训练模块必须至少保留一个有效训练单元");
      return;
    }
    const categoryIds = (prompt("分类 ID，多个用逗号分隔", current.categoryIds.join(",")) || "")
      .split(",").map((item) => item.trim()).filter((categoryId) => data.categories.some((item) => item.id === categoryId && !item.trashed));
    update({
      ...data,
      modules: data.modules.map((item) => item.id === id ? {
        ...item,
        name: name.trim(),
        description,
        unitIds: [...new Set(unitIds)],
        categoryIds: [...new Set(categoryIds)],
        updatedAt: new Date().toISOString(),
      } : item),
    }, "训练模块已修改");
  };

  const mergeCategoryAction = () => {
    try {
      const source = prompt("要合并并移除的源分类 ID");
      const target = prompt("保留的目标分类 ID");
      if (!source || !target) return;
      update(mergeCategories(data, source, target), "分类已合并");
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "分类合并失败");
    }
  };

  const mergeModuleAction = () => {
    try {
      const source = prompt("要合并并移除的源模块 ID");
      const target = prompt("保留的目标模块 ID");
      if (!source || !target) return;
      update(mergeModules(data, source, target), "训练模块已合并，训练单元已经去重");
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "模块合并失败");
    }
  };

  const exportAction = () => {
    const blob = new Blob([exportWorkspace(data)], { type: "application/json" });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = `loop-workspace-${new Date().toISOString().slice(0, 10)}.json`;
    link.click();
    URL.revokeObjectURL(link.href);
  };

  const importAction = async (file: File) => {
    try {
      const next = importWorkspace(await file.text());
      update(next, `导入完成：${next.categories.length} 个分类，${next.modules.length} 个训练模块`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "导入失败");
    }
  };

  return (
    <div className="manager-grid">
      <section className="manager-column">
        <div className="manager-heading">
          <div><h2>训练分类</h2><p>分类只组织训练模块，不改变知识库结构。</p></div>
          <button className="secondary" onClick={addCategory}>新增分类</button>
        </div>
        {categories.map((item) => (
          <article className="manager-card" key={item.id}>
            <div><strong>{item.name}</strong><small>{item.parentId ? `上级：${categoryNames.get(item.parentId) || "已移除"}` : "顶级分类"}</small><code>{item.id}</code></div>
            <div className="mini-actions">
              {view === "active" && <button onClick={() => editCategory(item.id)}>修改</button>}
              <button onClick={() => update({ ...data, categories: data.categories.map((entry) => entry.id === item.id ? { ...entry, trashed: view === "active" } : entry) }, view === "active" ? "分类已移入回收站，模块未删除" : "分类已恢复")}>{view === "active" ? "删除" : "恢复"}</button>
            </div>
          </article>
        ))}
        {!categories.length && <div className="empty-state">当前没有分类。</div>}
        {view === "active" && <button className="text-action" onClick={mergeCategoryAction}>合并两个分类</button>}
      </section>

      <section className="manager-column">
        <div className="manager-heading">
          <div><h2>我的训练模块</h2><p>把用户选择的训练单元组合成自己的训练入口。</p></div>
          <button className="secondary" onClick={addModule}>新增模块</button>
        </div>
        {modules.map((item) => (
          <article className="manager-card" key={item.id}>
            <div><strong>{item.name}</strong><small>{item.categoryIds.map((id) => categoryNames.get(id)).filter(Boolean).join(" / ") || "未分类"}</small><code>{item.id}</code></div>
            <div className="mini-actions">
              {view === "active" && <button onClick={() => editModule(item.id)}>修改</button>}
              {view === "active" && item.unitIds[0] && <button onClick={() => {
                const unit = catalogUnits.find((entry) => entry.id === item.unitIds[0]);
                if (unit) onTrain(unit);
              }}>训练</button>}
              <button onClick={() => update({ ...data, modules: data.modules.map((entry) => entry.id === item.id ? { ...entry, trashed: view === "active" } : entry) }, view === "active" ? "模块已移入回收站" : "模块已恢复")}>{view === "active" ? "删除" : "恢复"}</button>
            </div>
          </article>
        ))}
        {!modules.length && <div className="empty-state">当前没有用户训练模块，可从两个示范单元开始创建。</div>}
        {view === "active" && <button className="text-action" onClick={mergeModuleAction}>合并两个训练模块</button>}
      </section>

      <div className="manager-toolbar">
        <button className="secondary" onClick={() => setView(view === "active" ? "trash" : "active")}>{view === "active" ? "查看回收站" : "返回有效项目"}</button>
        <button className="secondary" onClick={exportAction}>导出工作区</button>
        <button className="secondary" onClick={() => importRef.current?.click()}>导入工作区</button>
        <input ref={importRef} hidden type="file" accept="application/json" onChange={(event) => {
          const file = event.target.files?.[0];
          if (file) void importAction(file);
          event.target.value = "";
        }} />
        {message && <span className="manager-message">{message}</span>}
      </div>
    </div>
  );
}
