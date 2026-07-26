import Ajv2020 from "ajv/dist/2020.js";
import fs from "node:fs";
import path from "node:path";
import process from "node:process";

const root = process.cwd();
const bankRoot = path.join(root, "banks");
const readJson = (file) => JSON.parse(fs.readFileSync(file, "utf8"));
const ajv = new Ajv2020({ allErrors: true, strict: false });
const validateUnit = ajv.compile(readJson(path.join(root, "schemas", "unit.schema.json")));
const validateBook = ajv.compile(readJson(path.join(root, "schemas", "book.schema.json")));
const validateSources = ajv.compile(readJson(path.join(root, "schemas", "knowledge_sources.schema.json")));
const validateRelease = ajv.compile(readJson(path.join(root, "schemas", "release.schema.json")));
const validateSecurity = ajv.compile(readJson(path.join(root, "schemas", "security.schema.json")));
const releasePath = path.join(root, "config", "release.json");
const securityPath = path.join(root, "config", "security.json");
const release = readJson(releasePath);
const security = readJson(securityPath);
const packageManifest = readJson(path.join(root, "package.json"));
const catalogPath = path.join(bankRoot, "index.json");
const catalog = readJson(catalogPath);
const errors = [];
const allUnitIds = new Set();
const allItemIds = new Set();
const allChapterIds = new Set();

function findFiles(directory, name, result = []) {
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const fullPath = path.join(directory, entry.name);
    if (entry.isDirectory()) findFiles(fullPath, name, result);
    else if (entry.name === name) result.push(fullPath);
  }
  return result;
}

function requireFields(item, fields, label) {
  for (const field of fields) if (!(field in item)) errors.push(`${label}: 缺少字段 ${field}`);
}

function requireJson(directory, relativePath, owner) {
  const file = path.join(directory, relativePath);
  if (!fs.existsSync(file)) {
    errors.push(`${owner}: 缺少 ${relativePath}`);
    return undefined;
  }
  try {
    return readJson(file);
  } catch (error) {
    errors.push(`${file}: JSON 无法解析：${error.message}`);
    return undefined;
  }
}

if (!validateRelease(release)) errors.push(`${releasePath}: ${ajv.errorsText(validateRelease.errors)}`);
if (!validateSecurity(security)) errors.push(`${securityPath}: ${ajv.errorsText(validateSecurity.errors)}`);
if (release.version !== packageManifest.version) errors.push(`${releasePath}: version 必须与 package.json 一致`);
if (packageManifest.license !== "GPL-2.0-only") errors.push(`${path.join(root, "package.json")}: 许可证必须为 GPL-2.0-only`);
if (release.product !== packageManifest.name) errors.push(`${releasePath}: product 必须与 package.json name 一致`);
const sourceExamplePath = path.join(root, "config", "knowledge_sources.example.json");
if (!validateSources(readJson(sourceExamplePath))) errors.push(`${sourceExamplePath}: ${ajv.errorsText(validateSources.errors)}`);

const trainingFields = {
  guided: ["id", "chapter_ids", "claim_ids", "title", "scenario", "question", "hints", "answer_framework", "common_mistakes", "knowledge_refs"],
  reconstruction: ["id", "chapter_ids", "claim_ids", "title", "output_type", "prompt", "constraints", "required_outputs", "verification_questions", "knowledge_refs"],
  professional: ["id", "chapter_ids", "claim_ids", "title", "difficulty", "background", "evidence", "questions", "rubric", "knowledge_refs"],
};

for (const unitPath of findFiles(bankRoot, "unit.json")) {
  const directory = path.dirname(unitPath);
  const unit = readJson(unitPath);
  allUnitIds.add(unit.id);
  if (!validateUnit(unit)) {
    errors.push(`${unitPath}: ${ajv.errorsText(validateUnit.errors)}`);
    continue;
  }
  const knownRefs = new Set(unit.knowledge_refs.map((item) => item.id));
  const bookPath = path.join(directory, unit.stages.learning.book_file);
  if (!fs.existsSync(bookPath)) {
    errors.push(`${unitPath}: 缺少电子书 ${unit.stages.learning.book_file}`);
    continue;
  }
  const book = readJson(bookPath);
  if (!validateBook(book)) errors.push(`${bookPath}: ${ajv.errorsText(validateBook.errors)}`);
  const chapterIds = new Set(book.chapters?.map((chapter) => chapter.id) ?? []);
  for (const chapter of book.chapters ?? []) {
    if (allChapterIds.has(chapter.id)) errors.push(`${bookPath}: 重复章节 ID ${chapter.id}`);
    allChapterIds.add(chapter.id);
    const chapterPath = path.join(directory, chapter.file);
    if (!fs.existsSync(chapterPath)) errors.push(`${bookPath}: 章节文件不存在 ${chapter.file}`);
    else if (fs.readFileSync(chapterPath, "utf8").trim().length < 300) errors.push(`${chapterPath}: 章节正文过短`);
    for (const prerequisite of chapter.prerequisites) if (!chapterIds.has(prerequisite)) errors.push(`${bookPath}: 前置章节不存在 ${prerequisite}`);
    for (const ref of chapter.knowledge_refs) if (!knownRefs.has(ref)) errors.push(`${bookPath}: 章节引用未在 unit.json 声明 ${ref}`);
  }
  const outlinePath = path.join(directory, book.outline_file);
  if (!fs.existsSync(outlinePath)) errors.push(`${bookPath}: 缺少目录大纲 ${book.outline_file}`);

  const claimsData = requireJson(directory, book.knowledge.claims_file, bookPath);
  const relationsData = requireJson(directory, book.knowledge.relations_file, bookPath);
  const sourceMap = requireJson(directory, book.knowledge.source_map_file, bookPath);
  const plan = requireJson(directory, book.training_plan, bookPath);
  const claims = claimsData?.claims ?? [];
  const claimIds = new Set(claims.map((claim) => claim.id));
  const evidenceIds = new Set((sourceMap?.evidence ?? []).map((entry) => entry.id));
  for (const claim of claims) {
    requireFields(claim, ["id", "statement", "type", "status", "authority_chapter", "evidence_ids"], `${book.knowledge.claims_file}:${claim.id}`);
    if (!chapterIds.has(claim.authority_chapter)) errors.push(`${bookPath}: 声明权威章节不存在 ${claim.authority_chapter}`);
    if (claim.status === "verified" && !claim.evidence_ids.length) errors.push(`${bookPath}: 已验证声明没有证据 ${claim.id}`);
    for (const evidenceId of claim.evidence_ids) if (!evidenceIds.has(evidenceId)) errors.push(`${bookPath}: 声明证据不存在 ${evidenceId}`);
  }
  for (const chapter of book.chapters ?? []) for (const claimId of chapter.claim_ids) if (!claimIds.has(claimId)) errors.push(`${bookPath}: 章节声明不存在 ${claimId}`);
  for (const evidence of sourceMap?.evidence ?? []) {
    if (!claimIds.has(evidence.claim_id)) errors.push(`${bookPath}: 证据声明不存在 ${evidence.claim_id}`);
    if (!knownRefs.has(evidence.knowledge_ref)) errors.push(`${bookPath}: 证据知识引用不存在 ${evidence.knowledge_ref}`);
  }
  const relationTypes = new Set(["requires", "enables", "causes", "prevents", "contradicts", "refines", "implements", "bounds", "trades_for", "alternative_to"]);
  for (const relation of relationsData?.relations ?? []) {
    if (!claimIds.has(relation.from) || !claimIds.has(relation.to)) errors.push(`${bookPath}: 知识关系端点不存在`);
    if (!relationTypes.has(relation.type)) errors.push(`${bookPath}: 未知知识关系 ${relation.type}`);
  }
  if (plan && (plan.book_id !== book.id || plan.book_version !== book.version)) errors.push(`${book.training_plan}: 电子书身份或版本不匹配`);
  const checks = plan ? requireJson(path.dirname(path.join(directory, book.training_plan)), plan.chapter_checks_file, book.training_plan) : undefined;
  const checkedChapters = new Set((checks ?? []).map((check) => check.chapter_id));
  for (const chapterId of chapterIds) if (!checkedChapters.has(chapterId)) errors.push(`${book.training_plan}: 章节缺少核验任务 ${chapterId}`);

  for (const stageName of ["guided", "reconstruction", "professional"]) {
    const stage = unit.stages[stageName];
    const itemPath = path.join(directory, stage.items_file);
    if (!fs.existsSync(itemPath)) {
      errors.push(`${unitPath}: 缺少 ${stage.items_file}`);
      continue;
    }
    const items = readJson(itemPath);
    if (!Array.isArray(items) || !items.length) errors.push(`${itemPath}: 必须包含至少一个训练任务`);
    for (const [index, item] of items.entries()) {
      const label = `${itemPath}[${index}]`;
      requireFields(item, trainingFields[stageName], label);
      if (allItemIds.has(item.id)) errors.push(`${label}: 重复训练题 ID ${item.id}`);
      allItemIds.add(item.id);
      for (const id of item.chapter_ids ?? []) if (!chapterIds.has(id)) errors.push(`${label}: 章节不存在 ${id}`);
      for (const id of item.claim_ids ?? []) if (!claimIds.has(id)) errors.push(`${label}: 声明不存在 ${id}`);
      for (const ref of item.knowledge_refs ?? []) if (!knownRefs.has(ref)) errors.push(`${label}: 知识引用未声明 ${ref}`);
    }
  }
}

for (const [index, item] of (catalog.units ?? []).entries()) {
  requireFields(item, ["id", "title", "summary", "domain", "topic", "module", "level", "estimated_minutes", "status", "unit_file", "tags"], `${catalogPath}[${index}]`);
  if (!fs.existsSync(path.join(bankRoot, item.unit_file ?? ""))) errors.push(`${catalogPath}[${index}]: 单元文件不存在 ${item.unit_file}`);
  if (!allUnitIds.has(item.id)) errors.push(`${catalogPath}[${index}]: 索引 ID 与单元文件不匹配 ${item.id}`);
}
for (const unitId of allUnitIds) if (!(catalog.units ?? []).some((item) => item.id === unitId)) errors.push(`${catalogPath}: 单元未登记 ${unitId}`);

if (errors.length) {
  console.error(errors.join("\n"));
  process.exit(1);
}
console.log(`内容检查通过：${allUnitIds.size} 个训练单元，${allChapterIds.size} 个电子书章节，${allItemIds.size} 个训练任务；声明、证据、关系与引用闭包有效。`);
