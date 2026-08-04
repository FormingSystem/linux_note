import { performance } from "node:perf_hooks";
import { markdown } from "@codemirror/lang-markdown";
import { EditorState } from "@codemirror/state";

function percentile_95(values) {
  const sorted = [...values].sort((left, right) => left - right);
  return sorted[Math.max(0, Math.ceil(sorted.length * 0.95) - 1)];
}

function benchmark(byte_size) {
  const prefix = "# benchmark\n";
  const content = prefix + "x".repeat(byte_size - Buffer.byteLength(prefix));
  const initialization_ms = [];
  const input_ms = [];
  for (let iteration = 0; iteration < 7; ++iteration) {
    const initialize_started = performance.now();
    const state = EditorState.create({ doc: content, extensions: [markdown()] });
    initialization_ms.push(performance.now() - initialize_started);
    const input_started = performance.now();
    const next_state = state.update({ changes: { from: state.doc.length, insert: "!" } }).state;
    input_ms.push(performance.now() - input_started);
    if (next_state.doc.length !== state.doc.length + 1) throw new Error("编辑事务没有生效");
  }
  return {
    byte_size,
    initialization_p95_ms: Number(percentile_95(initialization_ms).toFixed(3)),
    input_p95_ms: Number(percentile_95(input_ms).toFixed(3)),
  };
}

console.log(JSON.stringify({
  kind: "headless_codemirror_state",
  samples: 7,
  results: [benchmark(1024 * 1024), benchmark(5 * 1024 * 1024)],
}, null, 2));
