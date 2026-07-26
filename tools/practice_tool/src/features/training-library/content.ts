import catalogData from "../../../banks/index.json";
import type {
  GuidedQuestion,
  LearningGuide,
  LoadedUnit,
  ModelTask,
  PracticeUnit,
  ProfessionalCase,
  UnitCatalog,
  UnitCatalogItem,
} from "../../shared/types";

export const catalog = catalogData as UnitCatalog;

const unitFiles = import.meta.glob<PracticeUnit>("../../../banks/**/unit.json", { eager: true, import: "default" });
const learningFiles = import.meta.glob<LearningGuide[]>("../../../banks/**/learning_guides.json", { eager: true, import: "default" });
const guidedFiles = import.meta.glob<GuidedQuestion[]>("../../../banks/**/guided_questions.json", { eager: true, import: "default" });
const modelFiles = import.meta.glob<ModelTask[]>("../../../banks/**/model_tasks.json", { eager: true, import: "default" });
const caseFiles = import.meta.glob<ProfessionalCase[]>("../../../banks/**/professional_cases.json", { eager: true, import: "default" });

export function loadUnit(item: UnitCatalogItem): LoadedUnit {
  const unitPath = `../../../banks/${item.unit_file}`;
  const unit = unitFiles[unitPath];
  if (!unit) throw new Error(`训练单元不存在：${item.unit_file}`);
  const directory = unitPath.slice(0, unitPath.lastIndexOf("/") + 1);
  const learning = learningFiles[`${directory}${unit.stages.learning.items_file}`];
  const guided = guidedFiles[`${directory}${unit.stages.guided.items_file}`];
  const models = modelFiles[`${directory}${unit.stages.reconstruction.items_file}`];
  const cases = caseFiles[`${directory}${unit.stages.professional.items_file}`];
  if (!learning || !guided || !models || !cases) throw new Error(`训练单元阶段文件不完整：${item.id}`);
  return { unit, learning, guided, models, cases };
}
