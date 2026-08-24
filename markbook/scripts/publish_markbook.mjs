import { createHash, randomUUID } from "node:crypto";
import { spawnSync } from "node:child_process";
import {
  access,
  copyFile,
  mkdir,
  readFile,
  readdir,
  rename,
  rm,
  stat,
  writeFile
} from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import hljs from "highlight.js";
import { marked } from "marked";

const script_path = fileURLToPath(import.meta.url);
const script_directory = path.dirname(script_path);
const markbook_root = path.resolve(script_directory, "..");
const repository_root = path.resolve(markbook_root, "..");
const manifest_directory = path.join(markbook_root, "manifests");
const topic_directory = path.join(markbook_root, "topics");
const template_directory = path.join(markbook_root, "templates");
const runtime_directory = path.join(markbook_root, "runtime");
const generator_version = "1.1.0";
const highlight_js_version = "11.11.1";
const marked_version = "16.4.2";
const mermaid_version = "11.17.0";
const mermaid_filename = `mermaid-${mermaid_version}.min.js`;

marked.setOptions({
  gfm: true,
  breaks: false
});

marked.use({
  renderer: {
    code: render_code_block
  }
});

function fail(message, code = "MARKBOOK_ERROR") {
  const error = new Error(message);
  error.code = code;
  throw error;
}

async function path_exists(target_path) {
  try {
    await access(target_path);
    return true;
  } catch {
    return false;
  }
}

function sha256(content) {
  return createHash("sha256").update(content).digest("hex");
}

function to_posix_path(value) {
  return value.split(path.sep).join("/");
}

function escape_html(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function escape_attribute(value) {
  return escape_html(value);
}

function normalize_code_language(value) {
  const language = String(value || "")
    .trim()
    .split(/\s+/, 1)[0]
    .toLocaleLowerCase("en-US");
  return /^[a-z0-9][a-z0-9_+-]{0,63}$/u.test(language) ? language : "";
}

function render_code_block({ text, lang }) {
  const code_language = normalize_code_language(lang);
  const source_text = String(text).replace(/\n$/u, "");
  if (code_language === "mermaid") {
    return `<pre><code class="language-mermaid">${escape_html(source_text)}\n</code></pre>\n`;
  }

  const highlight_language = code_language && hljs.getLanguage(code_language)
    ? code_language
    : "plaintext";
  const highlighted_code = hljs.highlight(source_text, {
    language: highlight_language,
    ignoreIllegals: true
  }).value;
  const language_class = code_language || "plaintext";
  const language_label = code_language
    ? ` data-language="${escape_attribute(code_language)}"`
    : "";
  return `<pre class="code_block"${language_label}><code class="hljs language-${escape_attribute(language_class)}">${highlighted_code}\n</code></pre>\n`;
}

function strip_markdown_inline(value) {
  return value
    .replaceAll("\\_", "\uE000")
    .replaceAll("\\*", "*")
    .replace(/`([^`]*)`/g, "$1")
    .replace(/!\[([^\]]*)\]\([^)]*\)/g, "$1")
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .replace(/[~*_]/g, "")
    .replaceAll("\uE000", "_")
    .replace(/<[^>]+>/g, "")
    .trim();
}

function heading_slug(value) {
  return strip_markdown_inline(value)
    .normalize("NFKC")
    .toLocaleLowerCase("zh-CN")
    .replace(/[\u0000-\u001f\u007f]/g, "")
    .replace(/[!"#$%&'()*+,\/:;<=>?@[\\\]^`{|}~，。；：！？、“”‘’（）【】《》]/g, "")
    .trim()
    .replace(/\s+/g, "-");
}

function safe_identifier(value) {
  return value
    .normalize("NFKC")
    .toLocaleLowerCase("zh-CN")
    .replace(/[^\p{Letter}\p{Number}_-]+/gu, "-")
    .replace(/^-+|-+$/g, "") || "document";
}

function parse_front_matter(content, source_path) {
  const normalized = content.replace(/^\uFEFF/, "").replaceAll("\r\n", "\n");
  if (!normalized.startsWith("---\n")) {
    fail(`源文档缺少 Front Matter：${source_path}`, "INVALID_SOURCE_METADATA");
  }
  const closing_index = normalized.indexOf("\n---\n", 4);
  if (closing_index < 0) {
    fail(`源文档 Front Matter 未闭合：${source_path}`, "INVALID_SOURCE_METADATA");
  }
  const front_matter = normalized.slice(4, closing_index);
  const body = normalized.slice(closing_index + 5);
  const metadata = {};
  for (const line of front_matter.split("\n")) {
    const match = line.match(/^([a-z_][a-z0-9_-]*):\s*(.*)$/i);
    if (!match) {
      continue;
    }
    let field_value = match[2].trim();
    if ((field_value.startsWith('"') && field_value.endsWith('"')) ||
        (field_value.startsWith("'") && field_value.endsWith("'"))) {
      field_value = field_value.slice(1, -1);
    }
    metadata[match[1]] = field_value;
  }
  for (const required_field of ["id", "title", "kind", "status"]) {
    if (!metadata[required_field]) {
      fail(`源文档缺少 ${required_field}：${source_path}`, "INVALID_SOURCE_METADATA");
    }
  }
  return { metadata, body, normalized };
}

function extract_headings(markdown_body, article_id) {
  const headings = [];
  const slug_counts = new Map();
  let fence_marker = null;
  for (const line of markdown_body.split("\n")) {
    const fence_match = line.match(/^\s*(`{3,}|~{3,})/);
    if (fence_match) {
      const marker_character = fence_match[1][0];
      if (!fence_marker) {
        fence_marker = marker_character;
      } else if (fence_marker === marker_character) {
        fence_marker = null;
      }
      continue;
    }
    if (fence_marker) {
      continue;
    }
    const heading_match = line.match(/^(#{1,6})\s+(.+?)\s*#*\s*$/);
    if (!heading_match) {
      continue;
    }
    const depth = heading_match[1].length;
    const raw_title = heading_match[2];
    const plain_title = strip_markdown_inline(raw_title);
    const source_slug = heading_slug(raw_title) || `section-${headings.length + 1}`;
    const slug_count = slug_counts.get(source_slug) || 0;
    slug_counts.set(source_slug, slug_count + 1);
    const unique_slug = slug_count === 0 ? source_slug : `${source_slug}-${slug_count}`;
    const anchor = headings.length === 0 && depth === 1
      ? article_id
      : `${article_id}--${unique_slug}`;
    headings.push({ depth, raw_title, plain_title, source_slug, unique_slug, anchor });
  }
  return headings;
}

function build_heading_lookup(document_entry) {
  const lookup = new Map();
  for (const heading of document_entry.headings) {
    const candidates = [
      heading.source_slug,
      heading.unique_slug,
      heading.raw_title,
      heading.raw_title.replaceAll("\\_", "_"),
      heading.plain_title,
      heading_slug(heading.plain_title)
    ];
    for (const candidate of candidates) {
      const normalized = normalize_fragment(candidate);
      if (normalized && !lookup.has(normalized)) {
        lookup.set(normalized, heading.anchor);
      }
    }
  }
  document_entry.heading_lookup = lookup;
}

function normalize_fragment(value) {
  if (!value) {
    return "";
  }
  let decoded_value = value;
  try {
    decoded_value = decodeURIComponent(value);
  } catch {
    decoded_value = value;
  }
  return decoded_value
    .replace(/^#/, "")
    .replaceAll("\\_", "_")
    .normalize("NFKC")
    .toLocaleLowerCase("zh-CN")
    .trim();
}

function render_markdown_headings(markdown_body, document_entry) {
  const output_lines = [];
  let fence_marker = null;
  let heading_index = 0;
  for (const line of markdown_body.split("\n")) {
    const fence_match = line.match(/^\s*(`{3,}|~{3,})/);
    if (fence_match) {
      const marker_character = fence_match[1][0];
      if (!fence_marker) {
        fence_marker = marker_character;
      } else if (fence_marker === marker_character) {
        fence_marker = null;
      }
      output_lines.push(line);
      continue;
    }
    if (!fence_marker) {
      const heading_match = line.match(/^(#{1,6})\s+(.+?)\s*#*\s*$/);
      if (heading_match) {
        const heading = document_entry.headings[heading_index];
        heading_index += 1;
        if (heading_index === 1 && heading?.depth === 1) {
          continue;
        }
        const inline_title = marked.parseInline(heading?.raw_title || heading_match[2]);
        output_lines.push(`<h${heading.depth} id="${escape_attribute(heading.anchor)}">${inline_title}</h${heading.depth}>`);
        continue;
      }
    }
    output_lines.push(line);
  }
  return output_lines.join("\n");
}

function sanitize_rendered_html(html) {
  return html
    .replace(/<(script|style|iframe|object|embed|form|meta|link)\b[^>]*>[\s\S]*?<\/\1\s*>/gi, "")
    .replace(/<(script|style|iframe|object|embed|form|meta|link)\b[^>]*\/?>/gi, "")
    .replace(/\s+on[a-z]+\s*=\s*("[^"]*"|'[^']*'|[^\s>]+)/gi, "")
    .replace(/\s+(href|src)\s*=\s*(["'])\s*(?:javascript|vbscript):[\s\S]*?\2/gi, " $1=\"#blocked-link\"");
}

function is_external_url(value) {
  return /^(?:[a-z][a-z0-9+.-]*:|\/\/)/i.test(value);
}

function resolve_repository_path(repository_path) {
  if (typeof repository_path !== "string" || repository_path.length === 0) {
    fail("manifest 中存在空路径", "INVALID_MANIFEST");
  }
  const normalized_path = repository_path.replaceAll("\\", "/");
  if (path.posix.isAbsolute(normalized_path) || /^[a-z]:/i.test(normalized_path) || normalized_path.startsWith("//")) {
    fail(`manifest 不允许绝对路径：${repository_path}`, "INVALID_MANIFEST");
  }
  const path_segments = normalized_path.split("/");
  if (path_segments.includes("..") || path_segments.includes(".")) {
    fail(`manifest 路径不得包含 . 或 ..：${repository_path}`, "INVALID_MANIFEST");
  }
  const absolute_path = path.resolve(repository_root, ...path_segments);
  const relative_path = path.relative(repository_root, absolute_path);
  if (relative_path.startsWith("..") || path.isAbsolute(relative_path)) {
    fail(`manifest 路径越出仓库：${repository_path}`, "INVALID_MANIFEST");
  }
  return { absolute_path, repository_path: to_posix_path(relative_path) };
}

function rewrite_url(value, current_document, document_by_path, release_directory) {
  if (!value || is_external_url(value) || value.startsWith("data:")) {
    return value;
  }
  const hash_index = value.indexOf("#");
  const path_part = hash_index >= 0 ? value.slice(0, hash_index) : value;
  const fragment = hash_index >= 0 ? value.slice(hash_index + 1) : "";

  if (!path_part) {
    const target_anchor = current_document.heading_lookup.get(normalize_fragment(fragment));
    return target_anchor ? `#${target_anchor}` : `#${current_document.article_id}`;
  }

  let decoded_path = path_part;
  try {
    decoded_path = decodeURI(path_part);
  } catch {
    decoded_path = path_part;
  }
  const target_repository_path = path.posix.normalize(path.posix.join(
    path.posix.dirname(current_document.repository_path),
    decoded_path.replaceAll("\\", "/")
  ));
  const included_document = document_by_path.get(target_repository_path);
  if (included_document) {
    const target_anchor = fragment
      ? included_document.heading_lookup.get(normalize_fragment(fragment)) || included_document.article_id
      : included_document.article_id;
    return `#${target_anchor}`;
  }

  const target_absolute_path = path.resolve(repository_root, ...target_repository_path.split("/"));
  let relative_href = to_posix_path(path.relative(release_directory, target_absolute_path));
  if (fragment) {
    relative_href += `#${fragment}`;
  }
  return relative_href;
}

function render_document_body(document_entry, document_by_path, release_directory) {
  const markdown_with_anchors = render_markdown_headings(document_entry.body, document_entry);
  let html = marked.parse(markdown_with_anchors);
  html = html.replace(
    /<pre><code class="language-mermaid">([\s\S]*?)<\/code><\/pre>/g,
    '<pre class="mermaid">$1</pre>'
  );
  html = html.replace(/\b(href|src)="([^"]*)"/g, (match, attribute_name, attribute_value) => {
    const rewritten_url = rewrite_url(
      attribute_value.replaceAll("&amp;", "&"),
      document_entry,
      document_by_path,
      release_directory
    );
    return `${attribute_name}="${escape_attribute(rewritten_url)}"`;
  });
  return sanitize_rendered_html(html);
}

function validate_manifest(manifest, manifest_path) {
  const required_fields = [
    "schema_version",
    "topic_id",
    "enabled",
    "title",
    "subtitle",
    "description",
    "publisher",
    "language",
    "source_baseline",
    "volumes",
    "references"
  ];
  for (const required_field of required_fields) {
    if (manifest[required_field] === undefined || manifest[required_field] === null) {
      fail(`manifest 缺少 ${required_field}：${manifest_path}`, "INVALID_MANIFEST");
    }
  }
  if (manifest.schema_version !== 1) {
    fail(`不支持的 manifest schema_version：${manifest.schema_version}`, "INVALID_MANIFEST");
  }
  if (!/^[a-z0-9]+(?:_[a-z0-9]+)*$/.test(manifest.topic_id)) {
    fail(`非法 topic_id：${manifest.topic_id}`, "INVALID_MANIFEST");
  }
  if (!Array.isArray(manifest.volumes) || manifest.volumes.length === 0) {
    fail(`manifest 至少需要一个分卷：${manifest_path}`, "INVALID_MANIFEST");
  }
  const volume_ids = new Set();
  for (const volume of manifest.volumes) {
    if (!volume.id || !volume.title || !volume.description || !Array.isArray(volume.sources) || volume.sources.length === 0) {
      fail(`manifest 分卷字段不完整：${manifest_path}`, "INVALID_MANIFEST");
    }
    if (volume_ids.has(volume.id)) {
      fail(`manifest 分卷 id 重复：${volume.id}`, "INVALID_MANIFEST");
    }
    volume_ids.add(volume.id);
    for (const source_selector of volume.sources) {
      if (!source_selector.role || !["guide", "chapter", "appendix"].includes(source_selector.role)) {
        fail(`manifest source role 非法：${volume.id}`, "INVALID_MANIFEST");
      }
      if (source_selector.path) {
        resolve_repository_path(source_selector.path);
      } else if (source_selector.directory && source_selector.include === "P*.md") {
        resolve_repository_path(source_selector.directory);
      } else {
        fail(`manifest source selector 非法：${volume.id}`, "INVALID_MANIFEST");
      }
    }
    for (const attachment_path of volume.attachments || []) {
      resolve_repository_path(attachment_path);
    }
  }
  for (const reference of manifest.references) {
    if (!reference.title || !reference.path || !reference.reason) {
      fail(`manifest reference 字段不完整：${manifest_path}`, "INVALID_MANIFEST");
    }
    resolve_repository_path(reference.path);
  }
}

function shanghai_date_parts(current_date = new Date()) {
  const parts = new Intl.DateTimeFormat("en-CA", {
    timeZone: "Asia/Shanghai",
    year: "numeric",
    month: "2-digit",
    day: "2-digit"
  }).formatToParts(current_date);
  const part_map = Object.fromEntries(parts.map((part) => [part.type, part.value]));
  return {
    year: part_map.year,
    month: part_map.month,
    day: part_map.day,
    release_month: `${part_map.year}.${part_map.month}`
  };
}

function normalize_release_month(value) {
  if (!/^\d{4}\.(?:0[1-9]|1[0-2])$/.test(value || "")) {
    fail(`版本月份必须使用 YYYY.MM：${value || "<empty>"}`, "INVALID_RELEASE_MONTH");
  }
  return value;
}

function validate_release_policy({
  current_parts,
  release_month,
  initial,
  overwrite,
  existing_versions,
  target_exists,
  catalog_has_version
}) {
  const normalized_month = normalize_release_month(release_month || current_parts.release_month);
  if (initial && overwrite) {
    fail("--initial 与 --overwrite 不能同时使用", "INVALID_RELEASE_MODE");
  }
  if ((target_exists || catalog_has_version) && !overwrite) {
    fail(`版本 ${normalized_month} 已存在；同月发布不会重复生成`, "DUPLICATE_RELEASE");
  }
  if (overwrite) {
    if (!target_exists && !catalog_has_version) {
      fail(`--overwrite 只能覆盖已经存在的版本：${normalized_month}`, "MISSING_RELEASE");
    }
    return normalized_month;
  }
  if (initial) {
    if (existing_versions.length > 0) {
      fail("--initial 只允许尚无任何历史版本的专题", "INITIAL_RELEASE_EXISTS");
    }
    if (normalized_month !== current_parts.release_month) {
      fail("--initial 只能生成中国时区当前月份的首版", "INVALID_INITIAL_MONTH");
    }
    return normalized_month;
  }
  if (current_parts.day !== "01") {
    fail("常规 MarkBook 发布只允许在 Asia/Shanghai 每月 1 日执行", "NOT_MONTHLY_RELEASE_DAY");
  }
  if (normalized_month !== current_parts.release_month) {
    fail("常规发布只能生成中国时区当前月份", "INVALID_RELEASE_MONTH");
  }
  return normalized_month;
}

async function load_json(json_path) {
  return JSON.parse(await readFile(json_path, "utf8"));
}

async function load_catalog(topic_id) {
  const catalog_path = path.join(topic_directory, topic_id, "catalog.json");
  if (!await path_exists(catalog_path)) {
    return { schema_version: 1, topic_id, latest: null, releases: [] };
  }
  const catalog = await load_json(catalog_path);
  if (catalog.topic_id !== topic_id || !Array.isArray(catalog.releases)) {
    fail(`发行目录台账无效：${to_posix_path(path.relative(repository_root, catalog_path))}`, "INVALID_CATALOG");
  }
  return catalog;
}

async function list_existing_versions(topic_id) {
  const release_root = path.join(topic_directory, topic_id, "releases");
  if (!await path_exists(release_root)) {
    return [];
  }
  const entries = await readdir(release_root, { withFileTypes: true });
  return entries
    .filter((entry) => entry.isDirectory() && /^\d{4}\.(?:0[1-9]|1[0-2])$/.test(entry.name))
    .map((entry) => entry.name)
    .sort();
}

async function expand_source_selector(source_selector) {
  if (source_selector.path) {
    const resolved_path = resolve_repository_path(source_selector.path);
    const source_stat = await stat(resolved_path.absolute_path).catch(() => null);
    if (!source_stat?.isFile()) {
      fail(`manifest 源文件不存在：${source_selector.path}`, "MISSING_SOURCE");
    }
    return [{ ...resolved_path, role: source_selector.role }];
  }
  const resolved_directory = resolve_repository_path(source_selector.directory);
  const directory_stat = await stat(resolved_directory.absolute_path).catch(() => null);
  if (!directory_stat?.isDirectory()) {
    fail(`manifest 源目录不存在：${source_selector.directory}`, "MISSING_SOURCE_DIRECTORY");
  }
  const entries = await readdir(resolved_directory.absolute_path, { withFileTypes: true });
  const source_paths = entries
    .filter((entry) => entry.isFile() && /^P.+\.md$/u.test(entry.name))
    .sort((left, right) => left.name.localeCompare(right.name, "zh-CN", { numeric: true }))
    .map((entry) => ({
      absolute_path: path.join(resolved_directory.absolute_path, entry.name),
      repository_path: `${resolved_directory.repository_path}/${entry.name}`,
      role: source_selector.role
    }));
  if (source_paths.length === 0) {
    fail(`manifest 源目录没有匹配 ${source_selector.include}：${source_selector.directory}`, "EMPTY_SOURCE_SELECTOR");
  }
  return source_paths;
}

function read_git_snapshot_state() {
  const head_result = spawnSync("git", ["rev-parse", "HEAD"], {
    cwd: repository_root,
    encoding: "utf8"
  });
  if (head_result.status !== 0) {
    fail(`无法读取知识仓库 HEAD：${head_result.stderr.trim()}`, "GIT_STATE_ERROR");
  }
  const status_result = spawnSync(
    "git",
    ["-c", "core.quotepath=false", "status", "--porcelain=v1", "-z", "--untracked-files=all"],
    { cwd: repository_root, encoding: "utf8" }
  );
  if (status_result.status !== 0) {
    fail(`无法读取知识仓库工作树状态：${status_result.stderr.trim()}`, "GIT_STATE_ERROR");
  }
  const dirty_path_states = new Map();
  const records = status_result.stdout.split("\0");
  for (let record_index = 0; record_index < records.length; record_index += 1) {
    const record = records[record_index];
    if (!record) {
      continue;
    }
    const status_code = record.slice(0, 2);
    const repository_path = record.slice(3).replaceAll("\\", "/");
    dirty_path_states.set(repository_path, status_code);
    if (status_code.includes("R") || status_code.includes("C")) {
      record_index += 1;
    }
  }
  return {
    head: head_result.stdout.trim(),
    dirty_path_states
  };
}

async function load_documents(manifest) {
  const document_paths = new Set();
  const documents = [];
  let global_document_index = 0;
  for (let volume_index = 0; volume_index < manifest.volumes.length; volume_index += 1) {
    const volume = manifest.volumes[volume_index];
    let chapter_index = 0;
    for (const source_selector of volume.sources) {
      const expanded_sources = await expand_source_selector(source_selector);
      for (const source_path of expanded_sources) {
        if (document_paths.has(source_path.repository_path)) {
          fail(`manifest 重复收录源文档：${source_path.repository_path}`, "DUPLICATE_SOURCE");
        }
        document_paths.add(source_path.repository_path);
        const content_buffer = await readFile(source_path.absolute_path);
        const parsed_document = parse_front_matter(content_buffer.toString("utf8"), source_path.repository_path);
        const article_id = `document-${safe_identifier(parsed_document.metadata.id)}`;
        chapter_index += source_path.role === "guide" ? 0 : 1;
        const document_entry = {
          ...source_path,
          ...parsed_document,
          article_id,
          volume_id: volume.id,
          volume_title: volume.title,
          volume_index,
          document_index: global_document_index,
          chapter_index,
          display_label: source_path.role === "guide" ? "专题导读" : `第 ${chapter_index} 章`,
          content_sha256: sha256(content_buffer)
        };
        document_entry.headings = extract_headings(document_entry.body, article_id);
        build_heading_lookup(document_entry);
        documents.push(document_entry);
        global_document_index += 1;
      }
    }
  }
  const article_ids = new Set();
  const metadata_ids = new Set();
  for (const document_entry of documents) {
    if (article_ids.has(document_entry.article_id)) {
      fail(`生成锚点冲突：${document_entry.article_id}`, "DUPLICATE_DOCUMENT_ID");
    }
    if (metadata_ids.has(document_entry.metadata.id)) {
      fail(`收录文档 id 重复：${document_entry.metadata.id}`, "DUPLICATE_DOCUMENT_ID");
    }
    article_ids.add(document_entry.article_id);
    metadata_ids.add(document_entry.metadata.id);
  }
  return documents;
}

async function load_attachments(manifest) {
  const attachment_paths = new Set();
  const attachments = [];
  for (const volume of manifest.volumes) {
    for (const attachment_path of volume.attachments || []) {
      const resolved_path = resolve_repository_path(attachment_path);
      const attachment_stat = await stat(resolved_path.absolute_path).catch(() => null);
      if (!attachment_stat?.isFile()) {
        fail(`manifest 附件不存在：${attachment_path}`, "MISSING_ATTACHMENT");
      }
      if (attachment_paths.has(resolved_path.repository_path)) {
        fail(`manifest 重复收录附件：${attachment_path}`, "DUPLICATE_ATTACHMENT");
      }
      attachment_paths.add(resolved_path.repository_path);
      const content_buffer = await readFile(resolved_path.absolute_path);
      attachments.push({
        ...resolved_path,
        volume_id: volume.id,
        content_sha256: sha256(content_buffer),
        output_path: `attachments/${volume.id}/${path.basename(resolved_path.absolute_path)}`
      });
    }
  }
  return attachments;
}

async function load_manifest(manifest_path) {
  const manifest_buffer = await readFile(manifest_path);
  const manifest = JSON.parse(manifest_buffer.toString("utf8"));
  validate_manifest(manifest, to_posix_path(path.relative(repository_root, manifest_path)));
  return {
    manifest,
    manifest_buffer,
    manifest_path,
    manifest_repository_path: to_posix_path(path.relative(repository_root, manifest_path))
  };
}

async function discover_manifests(options) {
  if (options.all) {
    const entries = await readdir(manifest_directory, { withFileTypes: true });
    const manifest_paths = entries
      .filter((entry) => entry.isFile() && entry.name.endsWith(".json"))
      .map((entry) => path.join(manifest_directory, entry.name))
      .sort();
    const loaded_manifests = [];
    for (const manifest_path of manifest_paths) {
      const loaded_manifest = await load_manifest(manifest_path);
      if (loaded_manifest.manifest.enabled) {
        loaded_manifests.push(loaded_manifest);
      }
    }
    if (loaded_manifests.length === 0) {
      fail("没有启用的 MarkBook manifest", "NO_ENABLED_MANIFEST");
    }
    return loaded_manifests;
  }
  if (!options.topic) {
    fail("请使用 --topic <topic_id> 或 --all", "MISSING_TOPIC");
  }
  const manifest_path = path.join(manifest_directory, `${options.topic}.json`);
  if (!await path_exists(manifest_path)) {
    fail(`专题 manifest 不存在：${options.topic}`, "MISSING_MANIFEST");
  }
  return [await load_manifest(manifest_path)];
}

async function prepare_publication(loaded_manifest, options, current_date = new Date()) {
  const { manifest } = loaded_manifest;
  const current_parts = shanghai_date_parts(current_date);
  const catalog = await load_catalog(manifest.topic_id);
  const existing_versions = await list_existing_versions(manifest.topic_id);
  const requested_month = options.release_month || current_parts.release_month;
  const release_directory = path.join(topic_directory, manifest.topic_id, "releases", requested_month);
  const target_exists = await path_exists(release_directory);
  const catalog_has_version = catalog.releases.some((release) => release.version === requested_month);
  const release_month = validate_release_policy({
    current_parts,
    release_month: requested_month,
    initial: options.initial,
    overwrite: options.overwrite,
    existing_versions,
    target_exists,
    catalog_has_version
  });
  const documents = await load_documents(manifest);
  const attachments = await load_attachments(manifest);
  return {
    ...loaded_manifest,
    catalog,
    existing_versions,
    release_month,
    release_date: `${release_month.replace(".", "-")}-01`,
    release_directory,
    target_exists,
    documents,
    attachments
  };
}

function render_toc(publication_plan) {
  return publication_plan.manifest.volumes.map((volume, volume_index) => {
    const volume_documents = publication_plan.documents.filter((document_entry) => document_entry.volume_id === volume.id);
    const list_items = volume_documents.map((document_entry) => {
      const section_headings = document_entry.headings.filter((heading) => heading.depth === 2 || heading.depth === 3);
      const section_navigation = section_headings.length > 0
        ? `<details class="toc_sections">
            <summary>展开章内目录</summary>
            <ol class="toc_section_list">${section_headings.map((heading) => `
              <li class="toc_section_depth_${heading.depth}"><a data_toc_section_link href="#${escape_attribute(heading.anchor)}">${escape_html(heading.plain_title)}</a></li>`).join("")}
            </ol>
          </details>`
        : "";
      return `
      <li class="toc_document">
        <a class="toc_link" data_toc_link href="#${escape_attribute(document_entry.article_id)}">${escape_html(document_entry.display_label)}　${escape_html(document_entry.metadata.title)}</a>
        ${section_navigation}
      </li>`;
    }).join("");
    return `
      <details class="toc_volume" ${volume_index === 0 ? "open" : ""}>
        <summary>${escape_html(volume.title)}</summary>
        <ol class="toc_list">${list_items}</ol>
      </details>`;
  }).join("");
}

function render_chapter_navigation(documents, document_index) {
  const previous_document = documents[document_index - 1];
  const next_document = documents[document_index + 1];
  const previous_link = previous_document
    ? `<a href="#${escape_attribute(previous_document.article_id)}"><span>上一篇</span>${escape_html(previous_document.metadata.title)}</a>`
    : "<span></span>";
  const next_link = next_document
    ? `<a href="#${escape_attribute(next_document.article_id)}"><span>下一篇</span>${escape_html(next_document.metadata.title)}</a>`
    : "<span></span>";
  return `<nav class="chapter_navigation" aria-label="章节导航">${previous_link}${next_link}</nav>`;
}

function render_volumes(publication_plan, document_by_path) {
  return publication_plan.manifest.volumes.map((volume, volume_index) => {
    const volume_documents = publication_plan.documents.filter((document_entry) => document_entry.volume_id === volume.id);
    const chapter_html = volume_documents.map((document_entry) => {
      const body_html = render_document_body(
        document_entry,
        document_by_path,
        publication_plan.staging_directory
      );
      return `
        <article class="chapter" id="${escape_attribute(document_entry.article_id)}" data_chapter data-title="${escape_attribute(document_entry.metadata.title)}" data-volume="${escape_attribute(volume.title)}">
          <header class="chapter_header">
            <p class="chapter_eyebrow">${escape_html(volume.title)} · ${escape_html(document_entry.display_label)}</p>
            <h1 class="chapter_title">${escape_html(document_entry.metadata.title)}</h1>
            <p class="chapter_source">权威来源：${escape_html(document_entry.repository_path)}</p>
          </header>
          <div class="chapter_body">${body_html}</div>
          ${render_chapter_navigation(publication_plan.documents, document_entry.document_index)}
        </article>`;
    }).join("");
    return `
      <section class="volume" id="volume-${escape_attribute(volume.id)}">
        <header class="volume_introduction">
          <p class="volume_number">Volume ${volume_index + 1}</p>
          <h1 class="volume_title">${escape_html(volume.title)}</h1>
          <p class="volume_description">${escape_html(volume.description)}</p>
        </header>
        ${chapter_html}
      </section>`;
  }).join("");
}

function repository_href_from_release(repository_path, release_directory) {
  const target_path = resolve_repository_path(repository_path).absolute_path;
  return to_posix_path(path.relative(release_directory, target_path));
}

function render_references(publication_plan) {
  return publication_plan.manifest.references.map((reference) => {
    const href = repository_href_from_release(reference.path, publication_plan.staging_directory);
    return `<li><a href="${escape_attribute(href)}">${escape_html(reference.title)}</a>：${escape_html(reference.reason)}</li>`;
  }).join("");
}

function render_attachments(publication_plan) {
  if (publication_plan.attachments.length === 0) {
    return "<p>本期没有独立附件。</p>";
  }
  return `<ul>${publication_plan.attachments.map((attachment) => `
    <li><a href="${escape_attribute(attachment.output_path)}" download>${escape_html(path.basename(attachment.repository_path))}</a> <small>— ${escape_html(attachment.repository_path)}</small></li>`).join("")}</ul>`;
}

function normalize_generated_text(value) {
  return value.replace(/^[ \t]+$/gm, "");
}

function render_html(publication_plan, git_snapshot) {
  const manifest = publication_plan.manifest;
  const document_by_path = new Map(publication_plan.documents.map((document_entry) => [document_entry.repository_path, document_entry]));
  const source_dirty_paths = publication_plan.documents
    .filter((document_entry) => git_snapshot.dirty_path_states.has(document_entry.repository_path))
    .map((document_entry) => document_entry.repository_path);
  const attachment_dirty_paths = publication_plan.attachments
    .filter((attachment) => git_snapshot.dirty_path_states.has(attachment.repository_path))
    .map((attachment) => attachment.repository_path);
  const dirty_paths = [...source_dirty_paths, ...attachment_dirty_paths];
  const source_state_html = dirty_paths.length > 0
    ? `<p class="source_state_warning"><strong>快照提示：</strong>本期收录了 ${dirty_paths.length} 个当时尚未提交的源文件。` +
      "这些内容由逐文件 SHA-256 标识，不能仅用下方知识仓库 HEAD 还原。</p>"
    : "<p>本期所有已收录源文件在生成时均与知识仓库 HEAD 一致。</p>";
  const baseline = manifest.source_baseline;
  const runtime_href = to_posix_path(path.relative(
    publication_plan.staging_directory,
    path.join(runtime_directory, mermaid_filename)
  ));
  const license_href = to_posix_path(path.relative(publication_plan.staging_directory, path.join(repository_root, "LICENSE")));
  const review_map_href = repository_href_from_release("atlas/maps/knowledge_review_map.md", publication_plan.staging_directory);
  const generated_date_display = new Intl.DateTimeFormat("zh-CN", {
    timeZone: "Asia/Shanghai",
    dateStyle: "long",
    timeStyle: "short"
  }).format(new Date(publication_plan.generated_at));
  const release_display = publication_plan.release_month.replace(".", " 年 ") + " 月刊";
  const source_count = publication_plan.documents.length;
  const total_characters = publication_plan.documents.reduce(
    (total, document_entry) => total + document_entry.body.replace(/\s/g, "").length,
    0
  );
  const volume_html = render_volumes(publication_plan, document_by_path);

  return `<!doctype html>
<html lang="${escape_attribute(manifest.language)}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="color-scheme" content="light">
  <meta name="generator" content="linux-note MarkBook ${generator_version}">
  <meta name="description" content="${escape_attribute(manifest.description)}">
  <meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'none'; form-action 'none'; connect-src 'none'">
  <title>${escape_html(manifest.title)} · ${escape_html(publication_plan.release_month)}</title>
  <link rel="stylesheet" href="assets/markbook.css">
  <script defer src="${escape_attribute(runtime_href)}"></script>
  <script defer src="assets/markbook.js"></script>
</head>
<body data_sidebar_open="false">
  <a class="skip_link" href="#book_content">跳到正文</a>
  <div class="reading_progress" aria-hidden="true"><div class="reading_progress_bar" data_progress_bar></div></div>
  <header class="book_toolbar">
    <button class="toolbar_button menu_button" type="button" data_action="toggle_sidebar" aria-label="打开目录" aria-expanded="false">目录</button>
    <div class="book_toolbar_title" data_toolbar_title>${escape_html(manifest.title)}</div>
    <button class="toolbar_button" type="button" data_action="print">打印 / PDF</button>
  </header>
  <aside class="book_sidebar" aria-label="全书目录">
    <div class="sidebar_brand">
      <p class="sidebar_kicker">Linux Note · MarkBook</p>
      <p class="sidebar_title">${escape_html(manifest.title)}</p>
    </div>
    <div class="search_box">
      <input class="search_input" type="search" data_search_input placeholder="检索本期全文（Ctrl+K）" aria-label="检索本期全文">
      <div class="search_results" data_search_results data_visible="false" aria-live="polite"></div>
    </div>
    <nav class="book_toc" aria-label="全书目录">${render_toc(publication_plan)}</nav>
    <div class="sidebar_footer">${escape_html(release_display)} · ${source_count} 篇源文档</div>
  </aside>
  <button class="sidebar_backdrop" type="button" data_action="close_sidebar" aria-label="关闭目录"></button>
  <main class="book_shell" id="book_content">
    <section class="book_cover" aria-labelledby="cover_title">
      <div class="cover_content">
        <p class="cover_series">MarkBook · Linux Kernel Series</p>
        <h1 class="cover_title" id="cover_title">${escape_html(manifest.title)}</h1>
        <p class="cover_subtitle">${escape_html(manifest.subtitle)}</p>
        <div class="cover_meta">
          <div class="cover_meta_item"><span>版本</span><strong>${escape_html(release_display)}</strong></div>
          <div class="cover_meta_item"><span>出版维护</span><strong>${escape_html(manifest.publisher)}</strong></div>
          <div class="cover_meta_item"><span>源码基线</span><strong>${escape_html(baseline.kernel_version)}</strong></div>
          <div class="cover_meta_item"><span>文档规模</span><strong>${source_count} 篇 · ${total_characters.toLocaleString("zh-CN")} 字符</strong></div>
        </div>
      </div>
    </section>
    <section class="front_matter" id="reader_guide">
      <div class="front_matter_grid">
        <article class="editorial_card">
          <h2>本书定位</h2>
          <p>${escape_html(manifest.description)}</p>
          <p>本书是知识仓库的月度派生快照，不建立第二份可编辑正文。章节更正、补充与人工评审仍回到各自权威源文件完成。</p>
        </article>
        <article class="editorial_card">
          <h2>阅读方法</h2>
          <p>按卷连续阅读可以重建机制诞生、完善和源码兑现过程；也可以用左侧全文检索直接定位状态、函数、配置或误用边界。打印按钮使用 A4 出版样式生成 PDF。</p>
          <p><a href="${escape_attribute(review_map_href)}">查看权威材料的人工评审状态</a></p>
        </article>
        <article class="editorial_card source_baseline">
          <h2>Linux 源码身份与快照边界</h2>
          <dl>
            <dt>官方仓库</dt><dd>${escape_html(baseline.repository)}</dd>
            <dt>来源分支</dt><dd>${escape_html(baseline.branch)}</dd>
            <dt>发布标签</dt><dd>${escape_html(baseline.tag)}</dd>
            <dt>不可变提交</dt><dd><code>${escape_html(baseline.commit)}</code></dd>
            <dt>内核版本</dt><dd>${escape_html(baseline.kernel_version)}</dd>
            <dt>配置边界</dt><dd>${baseline.config.map((config) => `<code>${escape_html(config)}</code>`).join("、")}</dd>
            <dt>知识仓库 HEAD</dt><dd><code>${escape_html(git_snapshot.head)}</code></dd>
            <dt>生成时间</dt><dd>${escape_html(generated_date_display)}（Asia/Shanghai）</dd>
          </dl>
          ${source_state_html}
        </article>
      </div>
    </section>
    ${volume_html}
    <section class="book_colophon" id="references">
      <h2>延伸阅读、附件与版本说明</h2>
      <p>以下材料保持各自专题的权威职责，不在本书重复维护全文：</p>
      <ul class="reference_list">${render_references(publication_plan)}</ul>
      <h3>实验附件</h3>
      ${render_attachments(publication_plan)}
      <h3>版权与可追溯性</h3>
      <p>本期遵循知识仓库的 <a href="${escape_attribute(license_href)}">GPL-2.0-only 许可证</a>。未设置 ISBN，也不冒用商业出版社身份。完整来源哈希、工作树状态和产物哈希见同目录 <a href="publication.json">publication.json</a>。</p>
    </section>
  </main>
</body>
</html>`;
}

async function ensure_versioned_runtime() {
  await mkdir(runtime_directory, { recursive: true });
  const runtime_path = path.join(runtime_directory, mermaid_filename);
  if (await path_exists(runtime_path)) {
    return runtime_path;
  }
  const package_runtime_path = path.join(markbook_root, "node_modules", "mermaid", "dist", "mermaid.min.js");
  if (!await path_exists(package_runtime_path)) {
    fail("缺少 Mermaid 运行时；请先执行 npm ci --prefix markbook", "MISSING_DEPENDENCY");
  }
  await copyFile(package_runtime_path, runtime_path);
  return runtime_path;
}

async function hash_file(file_path) {
  return sha256(await readFile(file_path));
}

async function verify_html_links(html_path) {
  const html = await readFile(html_path, "utf8");
  const id_matches = [...html.matchAll(/\bid="([^"]+)"/g)].map((match) => match[1]);
  const id_set = new Set();
  const link_errors = [];
  for (const id_value of id_matches) {
    if (id_set.has(id_value)) {
      link_errors.push(`index.html: duplicate id ${id_value}`);
    }
    id_set.add(id_value);
  }
  const resource_matches = [...html.matchAll(/\b(?:href|src)="([^"]+)"/g)];
  for (const resource_match of resource_matches) {
    const raw_url = resource_match[1].replaceAll("&amp;", "&");
    if (!raw_url || is_external_url(raw_url) || raw_url.startsWith("data:")) {
      continue;
    }
    const hash_index = raw_url.indexOf("#");
    const path_part = hash_index >= 0 ? raw_url.slice(0, hash_index) : raw_url;
    const fragment = hash_index >= 0 ? normalize_fragment(raw_url.slice(hash_index + 1)) : "";
    if (!path_part) {
      if (fragment && !id_set.has(fragment)) {
        link_errors.push(`index.html: missing fragment #${fragment}`);
      }
      continue;
    }
    let decoded_path = path_part;
    try {
      decoded_path = decodeURI(path_part);
    } catch {
      decoded_path = path_part;
    }
    const target_path = path.resolve(path.dirname(html_path), ...decoded_path.replaceAll("\\", "/").split("/"));
    if (!await path_exists(target_path)) {
      link_errors.push(`index.html: missing target ${raw_url}`);
    }
  }
  return link_errors;
}

function decode_html_entities(value) {
  return value
    .replaceAll("&lt;", "<")
    .replaceAll("&gt;", ">")
    .replaceAll("&quot;", '"')
    .replaceAll("&#39;", "'")
    .replaceAll("&amp;", "&");
}

async function verify_mermaid_diagrams(html_path) {
  const html = await readFile(html_path, "utf8");
  const diagram_sources = [...html.matchAll(/<pre class="mermaid">([\s\S]*?)<\/pre>/g)]
    .map((match) => decode_html_entities(match[1]));
  if (diagram_sources.length === 0) {
    return { count: 0, errors: [] };
  }
  if (!globalThis.window || !globalThis.document) {
    const { JSDOM } = await import("jsdom");
    const validation_dom = new JSDOM("<!doctype html><html><body></body></html>");
    globalThis.window = validation_dom.window;
    globalThis.document = validation_dom.window.document;
    globalThis.DOMParser = validation_dom.window.DOMParser;
    globalThis.Element = validation_dom.window.Element;
    globalThis.HTMLElement = validation_dom.window.HTMLElement;
    globalThis.SVGElement = validation_dom.window.SVGElement;
  }
  const { default: mermaid } = await import("mermaid");
  const diagram_errors = [];
  for (let diagram_index = 0; diagram_index < diagram_sources.length; diagram_index += 1) {
    try {
      await mermaid.parse(diagram_sources[diagram_index]);
    } catch (error) {
      const first_error_line = String(error.message || error).split("\n")[0];
      diagram_errors.push(`diagram ${diagram_index + 1}: ${first_error_line}`);
    }
  }
  return { count: diagram_sources.length, errors: diagram_errors };
}

function source_state_for(repository_path, git_snapshot) {
  const status_code = git_snapshot.dirty_path_states.get(repository_path);
  if (!status_code) {
    return "clean";
  }
  if (status_code === "??") {
    return "untracked";
  }
  return `modified:${status_code.trim() || status_code}`;
}

async function write_publication(publication_plan, git_snapshot) {
  const topic_root = path.join(topic_directory, publication_plan.manifest.topic_id);
  const staging_name = `.staging-${publication_plan.release_month}-${randomUUID()}`;
  const staging_directory = path.join(topic_root, "releases", staging_name);
  publication_plan.staging_directory = staging_directory;
  publication_plan.generated_at = new Date().toISOString();
  await mkdir(path.join(staging_directory, "assets"), { recursive: true });

  try {
    const runtime_path = await ensure_versioned_runtime();
    await copyFile(path.join(template_directory, "markbook.css"), path.join(staging_directory, "assets", "markbook.css"));
    await copyFile(path.join(template_directory, "markbook.js"), path.join(staging_directory, "assets", "markbook.js"));
    for (const attachment of publication_plan.attachments) {
      const output_path = path.join(staging_directory, ...attachment.output_path.split("/"));
      await mkdir(path.dirname(output_path), { recursive: true });
      await copyFile(attachment.absolute_path, output_path);
    }

    const html = normalize_generated_text(render_html(publication_plan, git_snapshot));
    await writeFile(path.join(staging_directory, "index.html"), html, "utf8");

    const artifact_paths = [
      "index.html",
      "assets/markbook.css",
      "assets/markbook.js",
      ...publication_plan.attachments.map((attachment) => attachment.output_path)
    ];
    const artifacts = [];
    for (const artifact_path of artifact_paths) {
      const absolute_artifact_path = path.join(staging_directory, ...artifact_path.split("/"));
      const artifact_stat = await stat(absolute_artifact_path);
      artifacts.push({
        path: artifact_path,
        bytes: artifact_stat.size,
        sha256: await hash_file(absolute_artifact_path)
      });
    }
    const runtime_relative_path = to_posix_path(path.relative(staging_directory, runtime_path));
    const runtime_stat = await stat(runtime_path);
    artifacts.push({
      path: runtime_relative_path,
      bytes: runtime_stat.size,
      sha256: await hash_file(runtime_path),
      shared_runtime: true
    });

    const source_documents = publication_plan.documents.map((document_entry) => ({
      id: document_entry.metadata.id,
      title: document_entry.metadata.title,
      kind: document_entry.metadata.kind,
      maintenance_status: document_entry.metadata.status,
      path: document_entry.repository_path,
      volume_id: document_entry.volume_id,
      role: document_entry.role,
      bytes: Buffer.byteLength(document_entry.normalized, "utf8"),
      sha256: document_entry.content_sha256,
      worktree_state: source_state_for(document_entry.repository_path, git_snapshot)
    }));
    const source_attachments = publication_plan.attachments.map((attachment) => ({
      path: attachment.repository_path,
      volume_id: attachment.volume_id,
      output_path: attachment.output_path,
      sha256: attachment.content_sha256,
      worktree_state: source_state_for(attachment.repository_path, git_snapshot)
    }));
    const dirty_source_count = [...source_documents, ...source_attachments]
      .filter((source_entry) => source_entry.worktree_state !== "clean").length;
    const package_lock_path = path.join(markbook_root, "package-lock.json");
    const publication_record = {
      schema_version: 1,
      generator: {
        name: "linux-note-markbook",
        version: generator_version,
        path: "markbook/scripts/publish_markbook.mjs",
        sha256: await hash_file(script_path),
        dependency_lock: {
          path: "markbook/package-lock.json",
          sha256: await hash_file(package_lock_path)
        },
        publication_dependencies: {
          highlight_js: highlight_js_version,
          marked: marked_version,
          mermaid: mermaid_version
        }
      },
      topic_id: publication_plan.manifest.topic_id,
      title: publication_plan.manifest.title,
      version: publication_plan.release_month,
      release_date: publication_plan.release_date,
      generated_at: publication_plan.generated_at,
      timezone: "Asia/Shanghai",
      manifest: {
        path: publication_plan.manifest_repository_path,
        sha256: sha256(publication_plan.manifest_buffer)
      },
      knowledge_repository: {
        head: git_snapshot.head,
        source_state: dirty_source_count === 0 ? "clean" : "contains_uncommitted_sources",
        dirty_source_count
      },
      linux_source_baseline: publication_plan.manifest.source_baseline,
      counts: {
        volumes: publication_plan.manifest.volumes.length,
        documents: source_documents.length,
        attachments: source_attachments.length
      },
      source_documents,
      source_attachments,
      artifacts
    };
    await writeFile(
      path.join(staging_directory, "publication.json"),
      `${JSON.stringify(publication_record, null, 2)}\n`,
      "utf8"
    );

    await mkdir(path.dirname(publication_plan.release_directory), { recursive: true });
    let backup_directory = null;
    if (publication_plan.target_exists) {
      backup_directory = `${publication_plan.release_directory}.backup-${randomUUID()}`;
      await rename(publication_plan.release_directory, backup_directory);
    }
    try {
      await rename(staging_directory, publication_plan.release_directory);
    } catch (error) {
      if (backup_directory && await path_exists(backup_directory)) {
        await rename(backup_directory, publication_plan.release_directory);
      }
      throw error;
    }
    if (backup_directory) {
      await rm(backup_directory, { recursive: true, force: false });
    }

    const catalog_entry = {
      version: publication_plan.release_month,
      release_date: publication_plan.release_date,
      generated_at: publication_plan.generated_at,
      href: `releases/${publication_plan.release_month}/index.html`,
      documents: source_documents.length,
      attachments: source_attachments.length,
      source_state: publication_record.knowledge_repository.source_state
    };
    const catalog_releases = publication_plan.catalog.releases
      .filter((release) => release.version !== publication_plan.release_month);
    catalog_releases.push(catalog_entry);
    catalog_releases.sort((left, right) => right.version.localeCompare(left.version));
    const catalog = {
      schema_version: 1,
      topic_id: publication_plan.manifest.topic_id,
      title: publication_plan.manifest.title,
      latest: catalog_releases[0].version,
      releases: catalog_releases
    };
    await writeFile(path.join(topic_root, "catalog.json"), `${JSON.stringify(catalog, null, 2)}\n`, "utf8");
    const latest_html = `<!doctype html>\n<html lang="zh-CN"><head><meta charset="utf-8"><meta http-equiv="refresh" content="0; url=releases/${catalog.latest}/index.html"><title>${escape_html(publication_plan.manifest.title)}</title></head><body><p><a href="releases/${catalog.latest}/index.html">打开 ${escape_html(catalog.latest)} 版本</a></p></body></html>\n`;
    await writeFile(path.join(topic_root, "latest.html"), latest_html, "utf8");
    return publication_record;
  } catch (error) {
    if (await path_exists(staging_directory)) {
      await rm(staging_directory, { recursive: true, force: false });
    }
    throw error;
  }
}

async function publish(options) {
  const loaded_manifests = await discover_manifests(options);
  const publication_plans = [];
  for (const loaded_manifest of loaded_manifests) {
    publication_plans.push(await prepare_publication(loaded_manifest, options));
  }
  const git_snapshot = read_git_snapshot_state();
  const publication_records = [];
  for (const publication_plan of publication_plans) {
    publication_records.push(await write_publication(publication_plan, git_snapshot));
  }
  for (const publication_record of publication_records) {
    console.log(
      `PUBLISHED ${publication_record.topic_id} ${publication_record.version}: ` +
      `${publication_record.counts.documents} documents, ${publication_record.counts.attachments} attachments, ` +
      `source_state=${publication_record.knowledge_repository.source_state}`
    );
  }
}

async function verify_topic(options, topic_id) {
  const catalog = await load_catalog(topic_id);
  const release_month = normalize_release_month(options.release_month || catalog.latest);
  const release_directory = path.join(topic_directory, topic_id, "releases", release_month);
  const publication_path = path.join(release_directory, "publication.json");
  if (!await path_exists(publication_path)) {
    fail(`版本台账不存在：${topic_id} ${release_month}`, "MISSING_PUBLICATION_RECORD");
  }
  const publication_record = await load_json(publication_path);
  const artifact_errors = [];
  for (const artifact of publication_record.artifacts || []) {
    const artifact_path = path.resolve(release_directory, ...artifact.path.split("/"));
    if (!await path_exists(artifact_path)) {
      artifact_errors.push(`${artifact.path}: missing`);
      continue;
    }
    const actual_hash = await hash_file(artifact_path);
    if (actual_hash !== artifact.sha256) {
      artifact_errors.push(`${artifact.path}: sha256 mismatch`);
    }
  }
  if (artifact_errors.length > 0) {
    fail(`产物验证失败：\n- ${artifact_errors.join("\n- ")}`, "ARTIFACT_VERIFICATION_FAILED");
  }
  const html_path = path.join(release_directory, "index.html");
  const link_errors = await verify_html_links(html_path);
  if (link_errors.length > 0) {
    fail(`书内链接验证失败：\n- ${link_errors.join("\n- ")}`, "LINK_VERIFICATION_FAILED");
  }
  const mermaid_verification = await verify_mermaid_diagrams(html_path);
  if (mermaid_verification.errors.length > 0) {
    fail(
      `Mermaid 语法验证失败：\n- ${mermaid_verification.errors.join("\n- ")}`,
      "MERMAID_VERIFICATION_FAILED"
    );
  }

  const source_changes = [];
  for (const source_entry of [
    ...(publication_record.source_documents || []),
    ...(publication_record.source_attachments || [])
  ]) {
    const source_path = resolve_repository_path(source_entry.path).absolute_path;
    if (!await path_exists(source_path)) {
      source_changes.push(`${source_entry.path}: missing`);
      continue;
    }
    const actual_hash = await hash_file(source_path);
    if (actual_hash !== source_entry.sha256) {
      source_changes.push(`${source_entry.path}: changed after publication`);
    }
  }
  if (source_changes.length > 0 && options.require_current_sources) {
    fail(`源文件已偏离本期快照：\n- ${source_changes.join("\n- ")}`, "SOURCE_SNAPSHOT_CHANGED");
  }
  console.log(
    `VERIFIED ${topic_id} ${release_month}: ${publication_record.artifacts.length} artifacts; ` +
    `${source_changes.length} current source differences; 0 broken local links; ` +
    `${mermaid_verification.count} Mermaid diagrams parsed`
  );
  if (source_changes.length > 0) {
    console.log("NOTICE 当前源文件已演进；本期 publication.json 仍可用于还原发行时快照。");
  }
}

async function verify(options) {
  let topic_ids = [];
  if (options.all) {
    const loaded_manifests = await discover_manifests({ ...options, all: true, topic: null });
    topic_ids = loaded_manifests.map((loaded_manifest) => loaded_manifest.manifest.topic_id);
  } else if (options.topic) {
    topic_ids = [options.topic];
  } else {
    fail("verify 需要 --topic <topic_id> 或 --all", "MISSING_TOPIC");
  }
  for (const topic_id of topic_ids) {
    await verify_topic(options, topic_id);
  }
}

function parse_options(argument_list) {
  const options = {
    all: false,
    initial: false,
    overwrite: false,
    require_current_sources: false,
    release_month: null,
    topic: null
  };
  for (let argument_index = 0; argument_index < argument_list.length; argument_index += 1) {
    const argument = argument_list[argument_index];
    if (argument === "--all") {
      options.all = true;
    } else if (argument === "--initial") {
      options.initial = true;
    } else if (argument === "--overwrite") {
      options.overwrite = true;
    } else if (argument === "--require-current-sources") {
      options.require_current_sources = true;
    } else if (argument === "--topic" || argument === "--release-month") {
      const option_value = argument_list[argument_index + 1];
      if (!option_value || option_value.startsWith("--")) {
        fail(`${argument} 缺少值`, "INVALID_ARGUMENT");
      }
      argument_index += 1;
      if (argument === "--topic") {
        options.topic = option_value;
      } else {
        options.release_month = option_value;
      }
    } else {
      fail(`未知参数：${argument}`, "INVALID_ARGUMENT");
    }
  }
  if (options.all && options.topic) {
    fail("--all 与 --topic 不能同时使用", "INVALID_ARGUMENT");
  }
  return options;
}

async function main() {
  const [command, ...argument_list] = process.argv.slice(2);
  if (!command || !["publish", "verify"].includes(command)) {
    fail(
      "用法：publish_markbook.mjs <publish|verify> [--topic <id>|--all] [--release-month YYYY.MM] [--initial|--overwrite]",
      "INVALID_COMMAND"
    );
  }
  const options = parse_options(argument_list);
  if (command === "publish") {
    await publish(options);
  } else {
    await verify(options);
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === script_path) {
  main().catch((error) => {
    console.error(`${error.code || "MARKBOOK_ERROR"}: ${error.message}`);
    process.exitCode = 1;
  });
}

export {
  heading_slug,
  normalize_generated_text,
  normalize_release_month,
  parse_front_matter,
  render_code_block,
  resolve_repository_path,
  shanghai_date_parts,
  validate_release_policy
};
