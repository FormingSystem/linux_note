import Ajv2020 from "ajv/dist/2020.js";
import fs from "node:fs";
import path from "node:path";
import process from "node:process";

const root = process.cwd();
const bankRoot = path.join(root, "banks");
const schema = JSON.parse(fs.readFileSync(path.join(root, "schemas", "unit.schema.json"), "utf8"));
const sourceSchema = JSON.parse(fs.readFileSync(path.join(root, "schemas", "knowledge_sources.schema.json"), "utf8"));
const ajv = new Ajv2020({ allErrors: true, strict: false });
const validateUnit = ajv.compile(schema);
const validateSources = ajv.compile(sourceSchema);
const releasePath = path.join(root, "config", "release.json");
const securityPath = path.join(root, "config", "security.json");
const release = JSON.parse(fs.readFileSync(releasePath, "utf8"));
const security = JSON.parse(fs.readFileSync(securityPath, "utf8"));
const packageManifest = JSON.parse(fs.readFileSync(path.join(root, "package.json"), "utf8"));
const validateRelease = ajv.compile(JSON.parse(fs.readFileSync(path.join(root, "schemas", "release.schema.json"), "utf8")));
const validateSecurity = ajv.compile(JSON.parse(fs.readFileSync(path.join(root, "schemas", "security.schema.json"), "utf8")));

function findFiles(directory, name, result = []) {
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const fullPath = path.join(directory, entry.name);
    if (entry.isDirectory()) findFiles(fullPath, name, result);
    else if (entry.name === name) result.push(fullPath);
  }
  return result;
}

const errors = [];
const allItemIds = new Set();
const allUnitIds = new Set();
const catalogPath = path.join(bankRoot, "index.json");
const catalog = JSON.parse(fs.readFileSync(catalogPath, "utf8"));
const sourceExamplePath = path.join(root, "config", "knowledge_sources.example.json");
const sourceExample = JSON.parse(fs.readFileSync(sourceExamplePath, "utf8"));
const requiredFields = {
  guided: ["id", "title", "scenario", "question", "hints", "answer_framework", "common_mistakes", "knowledge_refs"],
  reconstruction: ["id", "title", "output_type", "prompt", "constraints", "required_outputs", "verification_questions", "knowledge_refs"],
  professional: ["id", "title", "difficulty", "background", "evidence", "questions", "rubric", "knowledge_refs"]
};

if (!validateRelease(release)) {
  errors.push(`${releasePath}: ${ajv.errorsText(validateRelease.errors)}`);
}
if (!validateSecurity(security)) {
  errors.push(`${securityPath}: ${ajv.errorsText(validateSecurity.errors)}`);
}
if (release.version !== packageManifest.version) {
  errors.push(`${releasePath}: version 必须与 package.json 一致`);
}

if (!validateSources(sourceExample)) {
  errors.push(`${sourceExamplePath}: ${ajv.errorsText(validateSources.errors)}`);
}

for (const unitPath of findFiles(bankRoot, "unit.json")) {
  const directory = path.dirname(unitPath);
  const unit = JSON.parse(fs.readFileSync(unitPath, "utf8"));
  allUnitIds.add(unit.id);
  if (!validateUnit(unit)) {
    errors.push(`${unitPath}: ${ajv.errorsText(validateUnit.errors)}`);
    continue;
  }

  for (const [stageName, stage] of Object.entries(unit.stages)) {
    const itemPath = path.join(directory, stage.items_file);
    if (!fs.existsSync(itemPath)) {
      errors.push(`${unitPath}: 缺少 ${stage.items_file}`);
      continue;
    }
    const items = JSON.parse(fs.readFileSync(itemPath, "utf8"));
    if (!Array.isArray(items) || items.length === 0) {
      errors.push(`${itemPath}: 必须包含至少一个训练任务`);
      continue;
    }
    for (const [index, item] of items.entries()) {
      for (const field of requiredFields[stageName]) {
        if (!(field in item)) errors.push(`${itemPath}[${index}]: 缺少字段 ${field}`);
      }
      if (allItemIds.has(item.id)) errors.push(`${itemPath}[${index}]: 重复 ID ${item.id}`);
      allItemIds.add(item.id);
      for (const ref of item.knowledge_refs || []) {
        if (!unit.knowledge_refs.some((known) => known.id === ref)) {
          errors.push(`${itemPath}[${index}]: 未在 unit.json 声明知识引用 ${ref}`);
        }
      }
    }
  }
}

for (const [index, item] of (catalog.units || []).entries()) {
  const required = ["id", "title", "summary", "domain", "topic", "module", "level", "estimated_minutes", "status", "unit_file", "tags"];
  for (const field of required) {
    if (!(field in item)) errors.push(`${catalogPath}[${index}]: 缺少字段 ${field}`);
  }
  const declaredPath = path.join(bankRoot, item.unit_file || "");
  if (!fs.existsSync(declaredPath)) errors.push(`${catalogPath}[${index}]: 单元文件不存在 ${item.unit_file}`);
  if (!allUnitIds.has(item.id)) errors.push(`${catalogPath}[${index}]: 索引 ID 与单元文件不匹配 ${item.id}`);
}
for (const unitId of allUnitIds) {
  if (!(catalog.units || []).some((item) => item.id === unitId)) {
    errors.push(`${catalogPath}: 单元未登记到索引 ${unitId}`);
  }
}

if (errors.length) {
  console.error(errors.join("\n"));
  process.exit(1);
}

console.log(`内容检查通过：${allUnitIds.size} 个训练单元，${allItemIds.size} 个训练任务，索引、ID 与知识引用有效。`);
