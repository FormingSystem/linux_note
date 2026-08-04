import { performance } from "node:perf_hooks";
import {
  PREVIEW_PROTOCOL_VERSION,
  parse_markdown_preview,
} from "@loop/markdown-engine";

const MEBIBYTE = 1024 * 1024;
const SAMPLE_COUNT = 7;
const document_id = "benchmark_preview_document_0001";

function sized_source(byte_length, mode) {
  const paragraph = "This paragraph models ordinary prose with enough text to keep the fixture near a realistic node density. ".repeat(8);
  const block = mode === "structured"
    ? `## Heading\n\n${paragraph} **strong**, ~~delete~~, \`code\`, and [link](https://example.com).\n\n- item one\n- item two\n\n\`\`\`text\nfixed code\n\`\`\`\n\n`
    : "ordinary preview text ";
  const repetitions = Math.ceil(byte_length / Buffer.byteLength(block));
  return block.repeat(repetitions).slice(0, byte_length);
}

function line_dense_source(byte_length) {
  const line = "ordinary prose line with stable markdown text here.\n";
  const block = `${line.repeat(100)}\n`;
  const repetitions = Math.ceil(byte_length / Buffer.byteLength(block));
  return block.repeat(repetitions).slice(0, byte_length);
}

function percentile_95(values) {
  const sorted = [...values].sort((left, right) => left - right);
  return sorted[Math.ceil(sorted.length * 0.95) - 1] ?? 0;
}

function benchmark(label, source) {
  const samples = [];
  let node_count = 0;
  for (let index = 0; index < SAMPLE_COUNT; index += 1) {
    const started = performance.now();
    const result = parse_markdown_preview({
      message_type: "parse_preview",
      protocol_version: PREVIEW_PROTOCOL_VERSION,
      document_id,
      revision: index,
      source,
    });
    samples.push(performance.now() - started);
    node_count = result.node_count;
  }
  return {
    label,
    source_bytes: Buffer.byteLength(source),
    node_count,
    sample_count: SAMPLE_COUNT,
    p50_ms: Number([...samples].sort((left, right) => left - right)[Math.floor(samples.length / 2)]?.toFixed(3)),
    p95_ms: Number(percentile_95(samples).toFixed(3)),
  };
}

const measurements = [
  benchmark("1_mib_approximately_20000_lines", line_dense_source(MEBIBYTE)),
  benchmark("1_mib_structured", sized_source(MEBIBYTE, "structured")),
  benchmark("5_mib_plain", sized_source(5 * MEBIBYTE, "plain")),
];

console.log(JSON.stringify({ benchmark: "loop_d1c_preview", measurements }, null, 2));
