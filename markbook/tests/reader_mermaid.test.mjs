import test from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { JSDOM } from "jsdom";

const test_directory = path.dirname(fileURLToPath(import.meta.url));
const reader_script = await readFile(path.join(test_directory, "..", "templates", "markbook.js"), "utf8");

async function create_reader() {
  const dom = new JSDOM(`<!doctype html>
    <html><body data_sidebar_open="false">
      <button data_action="toggle_sidebar" aria-expanded="false">目录</button>
      <button data_action="print">打印</button>
      <button data_action="close_sidebar">关闭目录</button>
      <input data_search_input>
      <div data_search_results></div>
      <div data_progress_bar></div>
      <div data_toolbar_title></div>
      <a data_toc_link href="#chapter-1">章节</a>
      <article data_chapter id="chapter-1" data-title="测试章节" data-volume="测试卷">
        <pre class="mermaid">flowchart LR; A--&gt;B</pre>
      </article>
    </body></html>`, {
    pretendToBeVisual: true,
    runScripts: "outside-only",
    url: "https://markbook.test/"
  });
  const { window } = dom;
  let mermaid_configuration = null;

  window.IntersectionObserver = class {
    observe() {}
    disconnect() {}
  };
  window.ResizeObserver = class {
    observe() {}
    disconnect() {}
  };
  window.requestAnimationFrame = (callback) => window.setTimeout(() => callback(Date.now()), 0);
  window.cancelAnimationFrame = (identifier) => window.clearTimeout(identifier);
  window.HTMLElement.prototype.getBoundingClientRect = () => ({
    x: 0,
    y: 0,
    top: 0,
    right: 1000,
    bottom: 700,
    left: 0,
    width: 1000,
    height: 700,
    toJSON() { return this; }
  });
  window.SVGElement.prototype.getBBox = () => ({ x: 0, y: 0, width: 1600, height: 900 });
  window.mermaid = {
    initialize(configuration) {
      mermaid_configuration = configuration;
    },
    async run({ nodes }) {
      for (const node of nodes) {
        node.innerHTML = `<svg viewBox="0 0 1600 900" width="1600" height="900">
          <style>#arrow { fill: #123; }</style>
          <defs><marker id="arrow"></marker></defs>
          <path marker-end="url(#arrow)" d="M0 0 L100 100"></path>
        </svg>`;
      }
    }
  };
  window.eval(reader_script);
  await wait_for(() => window.document.querySelector(".mermaid_fullscreen_button"));
  return { dom, window, mermaid_configuration: () => mermaid_configuration };
}

async function wait_for(predicate) {
  for (let attempt = 0; attempt < 40; attempt += 1) {
    const value = predicate();
    if (value) {
      return value;
    }
    await new Promise((resolve) => setTimeout(resolve, 5));
  }
  throw new Error("等待 MarkBook Mermaid 阅读器初始化超时");
}

function pointer_event(window, type, x, y) {
  const event = new window.MouseEvent(type, {
    bubbles: true,
    button: 0,
    clientX: x,
    clientY: y
  });
  Object.defineProperty(event, "pointerId", { value: 7 });
  return event;
}

test("正文 Mermaid 保留自然尺寸并提供独立大图查看器", async () => {
  const { dom, window, mermaid_configuration } = await create_reader();
  const document = window.document;
  const figure = document.querySelector(".mermaid_diagram");
  const button = document.querySelector(".mermaid_fullscreen_button");
  const inline_svg = figure.querySelector("svg");

  assert.equal(mermaid_configuration().flowchart.useMaxWidth, false);
  assert.equal(mermaid_configuration().sequence.useMaxWidth, false);
  assert.equal(figure.querySelector(".mermaid_canvas .mermaid_content pre.mermaid"), document.querySelector("pre.mermaid"));
  assert.equal(inline_svg.getAttribute("width"), "1664");

  const inline_before_open = inline_svg.outerHTML;
  button.focus();
  button.click();
  await wait_for(() => !document.querySelector(".mermaid_viewer").hidden);

  const viewer = document.querySelector(".mermaid_viewer");
  const viewer_svg = viewer.querySelector("svg");
  const viewer_marker = viewer_svg.querySelector("marker");
  assert.equal(viewer.getAttribute("role"), "dialog");
  assert.equal(document.body.dataset.mermaid_viewer_open, "true");
  assert.notEqual(viewer_marker.id, "arrow");
  assert.equal(viewer_svg.querySelector("path").getAttribute("marker-end"), `url(#${viewer_marker.id})`);
  assert.match(viewer_svg.querySelector("style").textContent, new RegExp(`#${viewer_marker.id}`));

  viewer.querySelector("[data_mermaid_action='zoom_in']").click();
  assert.notEqual(viewer.querySelector("[data_mermaid_zoom]").textContent, "100%");
  window.document.dispatchEvent(new window.KeyboardEvent("keydown", { key: "Escape", bubbles: true }));

  assert.equal(viewer.hidden, true);
  assert.equal(document.body.dataset.mermaid_viewer_open, "false");
  assert.equal(inline_svg.outerHTML, inline_before_open);
  assert.equal(document.activeElement, button);
  dom.window.close();
});

test("大图查看器支持指针拖动和 Ctrl 滚轮缩放", async () => {
  const { dom, window } = await create_reader();
  const document = window.document;
  document.querySelector(".mermaid_fullscreen_button").click();
  const viewer = await wait_for(() => {
    const candidate = document.querySelector(".mermaid_viewer");
    return candidate && !candidate.hidden ? candidate : null;
  });
  const canvas = viewer.querySelector("[data_mermaid_canvas]");
  canvas.setPointerCapture = () => {};
  canvas.hasPointerCapture = () => false;

  canvas.dispatchEvent(pointer_event(window, "pointerdown", 300, 260));
  canvas.dispatchEvent(pointer_event(window, "pointermove", 390, 320));
  assert.equal(canvas.style.getPropertyValue("--mermaid_pan_x"), "90px");
  assert.equal(canvas.style.getPropertyValue("--mermaid_pan_y"), "60px");
  canvas.dispatchEvent(pointer_event(window, "pointerup", 390, 320));
  assert.equal(viewer.classList.contains("is_dragging"), false);

  const zoom_before_wheel = viewer.querySelector("[data_mermaid_zoom]").textContent;
  canvas.dispatchEvent(new window.WheelEvent("wheel", { bubbles: true, cancelable: true, deltaY: -120 }));
  assert.equal(viewer.querySelector("[data_mermaid_zoom]").textContent, zoom_before_wheel);
  canvas.dispatchEvent(new window.WheelEvent("wheel", {
    bubbles: true,
    cancelable: true,
    ctrlKey: true,
    clientX: 500,
    clientY: 350,
    deltaY: -120
  }));
  assert.notEqual(viewer.querySelector("[data_mermaid_zoom]").textContent, zoom_before_wheel);
  dom.window.close();
});
