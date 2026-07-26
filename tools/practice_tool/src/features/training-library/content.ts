import catalogData from "../../../banks/index.json";
import type {
  GuidedQuestion,
  KnowledgeClaim,
  LearningChapter,
  LoadedUnit,
  ModelTask,
  PracticeUnit,
  ProfessionalCase,
  UnitCatalog,
  UnitCatalogItem,
  TopicBook,
} from "../../shared/types";

export const catalog = catalogData as UnitCatalog;

const unitFiles = import.meta.glob<PracticeUnit>("../../../banks/**/unit.json", { eager: true, import: "default" });
type ChapterRecord = Omit<LearningChapter, "content_markdown" | "check_questions" | "open_associations" | "topology_memory">;
type BookRecord = Omit<TopicBook, "outline_markdown" | "claims" | "chapters"> & {
  chapters: ChapterRecord[];
  knowledge: { claims_file: string; relations_file: string; source_map_file: string };
  training_plan: string;
};
type ChapterCheck = Pick<LearningChapter, "check_questions" | "open_associations" | "topology_memory"> & { chapter_id: string };
const bookFiles = import.meta.glob<BookRecord>("../../../banks/**/book.json", { eager: true, import: "default" });
const chapterCheckFiles = import.meta.glob<ChapterCheck[]>("../../../banks/**/training/chapter_checks.json", { eager: true, import: "default" });
const claimFiles = import.meta.glob<{ schema_version: 1; claims: KnowledgeClaim[] }>("../../../banks/**/knowledge/claims.json", { eager: true, import: "default" });
const markdownFiles = import.meta.glob<string>("../../../banks/**/*.md", { eager: true, query: "?raw", import: "default" });
const guidedFiles = import.meta.glob<GuidedQuestion[]>("../../../banks/**/guided_questions.json", { eager: true, import: "default" });
const modelFiles = import.meta.glob<ModelTask[]>("../../../banks/**/model_tasks.json", { eager: true, import: "default" });
const caseFiles = import.meta.glob<ProfessionalCase[]>("../../../banks/**/professional_cases.json", { eager: true, import: "default" });

export function loadUnit(item: UnitCatalogItem): LoadedUnit {
  const unitPath = `../../../banks/${item.unit_file}`;
  const unit = unitFiles[unitPath];
  if (!unit) throw new Error(`训练单元不存在：${item.unit_file}`);
  const directory = unitPath.slice(0, unitPath.lastIndexOf("/") + 1);
  const bookRecord = bookFiles[`${directory}${unit.stages.learning.book_file}`];
  const guided = guidedFiles[`${directory}${unit.stages.guided.items_file}`];
  const models = modelFiles[`${directory}${unit.stages.reconstruction.items_file}`];
  const cases = caseFiles[`${directory}${unit.stages.professional.items_file}`];
  if (!bookRecord || !guided || !models || !cases) throw new Error(`训练单元内容文件不完整：${item.id}`);
  const checks = chapterCheckFiles[`${directory}training/chapter_checks.json`];
  const claimSet = claimFiles[`${directory}${bookRecord.knowledge.claims_file}`];
  const outlineMarkdown = markdownFiles[`${directory}${bookRecord.outline_file}`];
  if (!checks || !claimSet || !outlineMarkdown) throw new Error(`专题电子书治理文件不完整：${item.id}`);
  const checkMap = new Map(checks.map((check) => [check.chapter_id, check]));
  const learning = bookRecord.chapters.map((chapter) => {
    const contentMarkdown = markdownFiles[`${directory}${chapter.file}`];
    const check = checkMap.get(chapter.id);
    if (!contentMarkdown || !check) throw new Error(`电子书章节不完整：${chapter.id}`);
    return { ...chapter, ...check, content_markdown: contentMarkdown };
  });
  const book: TopicBook = { ...bookRecord, chapters: learning, outline_markdown: outlineMarkdown, claims: claimSet.claims };
  return { unit, book, learning, guided, models, cases };
}
