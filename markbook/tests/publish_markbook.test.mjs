import test from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { parse as parse_yaml } from "yaml";
import {
  heading_slug,
  normalize_generated_text,
  normalize_release_month,
  render_code_block,
  shanghai_date_parts,
  validate_release_policy
} from "../scripts/publish_markbook.mjs";

const test_directory = path.dirname(fileURLToPath(import.meta.url));
const repository_root = path.resolve(test_directory, "..", "..");

const first_day = {
  year: "2026",
  month: "09",
  day: "01",
  release_month: "2026.09"
};

test("中国时区日期决定月刊版本", () => {
  assert.deepEqual(shanghai_date_parts(new Date("2026-08-31T16:00:00.000Z")), first_day);
});

test("版本月份严格使用 YYYY.MM", () => {
  assert.equal(normalize_release_month("2026.09"), "2026.09");
  assert.throws(() => normalize_release_month("2026-9"), { code: "INVALID_RELEASE_MONTH" });
  assert.throws(() => normalize_release_month("2026.13"), { code: "INVALID_RELEASE_MONTH" });
});

test("常规发布只允许当月一号", () => {
  assert.equal(validate_release_policy({
    current_parts: first_day,
    release_month: "2026.09",
    initial: false,
    overwrite: false,
    existing_versions: ["2026.08"],
    target_exists: false,
    catalog_has_version: false
  }), "2026.09");

  assert.throws(() => validate_release_policy({
    current_parts: { ...first_day, day: "02" },
    release_month: "2026.09",
    initial: false,
    overwrite: false,
    existing_versions: ["2026.08"],
    target_exists: false,
    catalog_has_version: false
  }), { code: "NOT_MONTHLY_RELEASE_DAY" });
});

test("同专题同月在写入前拒绝重复发布", () => {
  assert.throws(() => validate_release_policy({
    current_parts: first_day,
    release_month: "2026.09",
    initial: false,
    overwrite: false,
    existing_versions: ["2026.09"],
    target_exists: true,
    catalog_has_version: true
  }), { code: "DUPLICATE_RELEASE" });
});

test("首刊豁免仅适用于当前月且没有历史版本", () => {
  assert.equal(validate_release_policy({
    current_parts: { ...first_day, day: "24" },
    release_month: "2026.09",
    initial: true,
    overwrite: false,
    existing_versions: [],
    target_exists: false,
    catalog_has_version: false
  }), "2026.09");

  assert.throws(() => validate_release_policy({
    current_parts: { ...first_day, day: "24" },
    release_month: "2026.09",
    initial: true,
    overwrite: false,
    existing_versions: ["2026.08"],
    target_exists: false,
    catalog_has_version: false
  }), { code: "INITIAL_RELEASE_EXISTS" });
});

test("覆盖必须显式且只能针对现有版本", () => {
  assert.equal(validate_release_policy({
    current_parts: { ...first_day, day: "24" },
    release_month: "2026.08",
    initial: false,
    overwrite: true,
    existing_versions: ["2026.08"],
    target_exists: true,
    catalog_has_version: true
  }), "2026.08");

  assert.throws(() => validate_release_policy({
    current_parts: { ...first_day, day: "24" },
    release_month: "2026.08",
    initial: false,
    overwrite: true,
    existing_versions: [],
    target_exists: false,
    catalog_has_version: false
  }), { code: "MISSING_RELEASE" });
});

test("标题锚点保留章节数字、下划线与中文", () => {
  assert.equal(heading_slug("1.9\\_建议的源码阅读顺序"), "1.9_建议的源码阅读顺序");
});

test("生成文本只清除空白行缩进", () => {
  assert.equal(normalize_generated_text("<p>正文  </p>\n    \n<pre>保留  </pre>"), "<p>正文  </p>\n\n<pre>保留  </pre>");
});

test("代码块在构建期生成 VS Code Light 可着色语法标记", () => {
  const html = render_code_block({
    text: "static int answer = 42;\nreturn answer;",
    lang: "c"
  });
  assert.match(html, /<pre class="code_block" data-language="c">/u);
  assert.match(html, /<code class="hljs language-c">/u);
  assert.match(html, /hljs-keyword/u);
  assert.match(html, /hljs-type/u);
  assert.match(html, /hljs-number/u);
});

test("未知代码语言可靠退化为纯文本且 Mermaid 保持独立渲染路径", () => {
  const unknown_html = render_code_block({
    text: "<unsafe>&value",
    lang: "kernel-log"
  });
  assert.match(unknown_html, /class="hljs language-kernel-log"/u);
  assert.match(unknown_html, /&lt;unsafe&gt;&amp;value/u);
  assert.doesNotMatch(unknown_html, /hljs-keyword/u);

  const mermaid_html = render_code_block({
    text: "flowchart LR\n  A --> B",
    lang: "mermaid"
  });
  assert.equal(
    mermaid_html,
    '<pre><code class="language-mermaid">flowchart LR\n  A --&gt; B\n</code></pre>\n'
  );
});

test("月度工作流 YAML 可解析并固定在每月一号", async () => {
  const workflow_content = await readFile(
    path.join(repository_root, ".github", "workflows", "markbook_monthly.yml"),
    "utf8"
  );
  const workflow = parse_yaml(workflow_content);
  assert.equal(workflow.on.schedule[0].cron, "0 0 1 * *");
  assert.equal(workflow.permissions.contents, "write");
  assert.equal(workflow.concurrency["cancel-in-progress"], false);
});
