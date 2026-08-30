const port = Number(process.argv[2] ?? "9223");
const screenshot_path = process.argv[3] ?? "typora_smoke.png";
const close_browser = process.argv.includes("--close");

async function wait_for_target() {
  for (let attempt = 0; attempt < 40; attempt += 1) {
    try {
      const targets = await fetch(`http://127.0.0.1:${port}/json`).then((response) => response.json());
      const page = targets.find((target) => target.type === "page" && target.webSocketDebuggerUrl);
      if (page) return page;
    } catch {
      // The isolated Typora renderer may still be starting.
    }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
  throw new Error(`Typora DevTools target was not available on port ${port}`);
}

const target = await wait_for_target();
const socket = new WebSocket(target.webSocketDebuggerUrl);
const pending = new Map();
let sequence = 0;

socket.addEventListener("message", (event) => {
  const message = JSON.parse(String(event.data));
  if (!message.id) return;
  const callback = pending.get(message.id);
  if (!callback) return;
  pending.delete(message.id);
  if (message.error) callback.reject(new Error(JSON.stringify(message.error)));
  else callback.resolve(message.result);
});

await new Promise((resolve, reject) => {
  socket.addEventListener("open", resolve, { once: true });
  socket.addEventListener("error", reject, { once: true });
});

function send(method, params = {}) {
  sequence += 1;
  const id = sequence;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    socket.send(JSON.stringify({ id, method, params }));
  });
}

async function evaluate(expression) {
  const result = await send("Runtime.evaluate", {
    expression,
    awaitPromise: true,
    returnByValue: true,
  });
  if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
  return result.result.value;
}

await send("Runtime.enable");
await send("Page.enable");
for (let attempt = 0; attempt < 40; attempt += 1) {
  const ready = await evaluate(`({
    state: document.readyState,
    enhancement: document.documentElement.getAttribute("data-linux-note-typora-enhancements"),
    fences: document.querySelectorAll(".md-fences").length
  })`);
  if (ready.state === "complete" && ready.enhancement === "ready" && ready.fences > 0) break;
  await new Promise((resolve) => setTimeout(resolve, 250));
}

const result = await evaluate(`(() => {
  const lines = Array.from(document.querySelectorAll('.md-fences[lang="c"] .CodeMirror-line'));
  const function_span = lines.flatMap((line) => Array.from(line.querySelectorAll('span')))
    .find((span) => span.textContent === "rcu_replace_pointer" || span.textContent === "call_rcu");
  const code_toggle = document.querySelector('.linux-note-code-toggle');
  const code_fence = code_toggle?.closest('.md-fences');
  const code_collapsed_before = Boolean(code_fence?.classList.contains('is-code-collapsed'));
  code_toggle?.click();
  const code_expanded_after = Boolean(code_fence?.classList.contains('is-code-expanded'));
  const code_expanded_label = code_toggle?.textContent?.trim() ?? null;
  const mermaid_button = document.querySelector('.linux-note-mermaid-open');
  const mermaid_toolbar = mermaid_button?.closest('.linux-note-mermaid-inline-toolbar');
  const mermaid_preview = mermaid_toolbar?.parentElement;
  const toolbar_position = mermaid_toolbar ? getComputedStyle(mermaid_toolbar).position : null;
  const duplicate_toolbars = Array.from(document.querySelectorAll('.md-fences'))
    .some((fence) => fence.querySelectorAll('.linux-note-mermaid-inline-toolbar').length > 1);
  mermaid_button?.click();
  return {
    enhancement: document.documentElement.getAttribute("data-linux-note-typora-enhancements"),
    c_mode: document.querySelector('.md-fences[lang="c"] .CodeMirror')?.CodeMirror?.getOption('mode') ?? null,
    function_text: function_span?.textContent ?? null,
    function_class: function_span?.className ?? null,
    code_toggles: document.querySelectorAll('.linux-note-code-toggle').length,
    code_collapsed_before,
    code_expanded_after,
    code_expanded_label,
    mermaid_buttons: document.querySelectorAll('.linux-note-mermaid-open').length,
    toolbar_inside_preview: Boolean(mermaid_preview?.matches('.md-diagram-panel-preview')),
    toolbar_position,
    duplicate_toolbars,
    viewer_open: Boolean(document.querySelector('.linux-note-mermaid-viewer')),
    viewer_svg: Boolean(document.querySelector('.linux-note-mermaid-viewer svg'))
  };
})()`);

const screenshot = await send("Page.captureScreenshot", { format: "png", captureBeyondViewport: false });
const fs = await import("node:fs");
fs.writeFileSync(screenshot_path, Buffer.from(screenshot.data, "base64"));
if (close_browser) await send("Browser.close");
else socket.close();

console.log(JSON.stringify({ target: target.title, ...result, screenshot_path }, null, 2));
if (result.enhancement !== "ready") throw new Error(`enhancement state is ${result.enhancement}`);
if (result.c_mode !== "linux-note-vscode-textmate-c") throw new Error(`unexpected C mode: ${result.c_mode}`);
if (!String(result.function_class).includes("cm-tm-function")) throw new Error(`function token class is ${result.function_class}`);
if (!result.code_toggles || !result.code_collapsed_before || !result.code_expanded_after || result.code_expanded_label !== "↥收起代码") {
  throw new Error(`long code collapse smoke check failed: ${JSON.stringify(result)}`);
}
if (!result.toolbar_inside_preview || result.toolbar_position !== "static" || result.duplicate_toolbars) {
  throw new Error(`Mermaid toolbar is not unique and static inside preview: ${JSON.stringify(result)}`);
}
if (!result.mermaid_buttons || !result.viewer_open || !result.viewer_svg) throw new Error("Mermaid viewer smoke check failed");
