const page_body = document.body;
const menu_button = document.querySelector("[data_action='toggle_sidebar']");
const close_targets = document.querySelectorAll("[data_action='close_sidebar']");
const search_input = document.querySelector("[data_search_input]");
const search_results = document.querySelector("[data_search_results]");
const progress_bar = document.querySelector("[data_progress_bar]");
const toolbar_title = document.querySelector("[data_toolbar_title]");
const chapter_elements = [...document.querySelectorAll("article[data_chapter]")];
const toc_links = [...document.querySelectorAll("[data_toc_link]")];
const toc_section_links = [...document.querySelectorAll("[data_toc_section_link]")];
const mermaid_min_zoom = 0.2;
const mermaid_max_zoom = 6;
let mermaid_viewer_sequence = 0;
let mermaid_viewer = null;
let mermaid_viewer_trigger = null;
let mermaid_viewer_resize_observer = null;

function set_sidebar(open_state) {
  page_body.dataset.sidebar_open = open_state ? "true" : "false";
  menu_button?.setAttribute("aria-expanded", open_state ? "true" : "false");
}

menu_button?.addEventListener("click", () => {
  set_sidebar(page_body.dataset.sidebar_open !== "true");
});

for (const close_target of close_targets) {
  close_target.addEventListener("click", () => set_sidebar(false));
}

document.querySelector("[data_action='print']")?.addEventListener("click", () => window.print());

function normalize_search_text(value) {
  return value.normalize("NFKC").toLocaleLowerCase("zh-CN").replace(/\s+/g, " ").trim();
}

const search_documents = chapter_elements.map((chapter) => {
  const text = (chapter.textContent || "").replace(/\s+/g, " ").trim();
  return {
    id: chapter.id,
    title: chapter.dataset.title || "",
    volume: chapter.dataset.volume || "",
    text,
    search_text: normalize_search_text(text)
  };
});

function create_search_result(document_entry, query) {
  const result_link = document.createElement("a");
  result_link.className = "search_result";
  result_link.href = `#${document_entry.id}`;
  const query_index = document_entry.search_text.indexOf(query);
  const start_index = Math.max(0, query_index - 34);
  const end_index = Math.min(document_entry.text.length, query_index + query.length + 52);
  const snippet = document_entry.text.slice(start_index, end_index);
  const prefix = start_index > 0 ? "…" : "";
  const suffix = end_index < document_entry.text.length ? "…" : "";

  const title_element = document.createElement("strong");
  title_element.textContent = document_entry.title;
  const snippet_element = document.createElement("small");
  snippet_element.textContent = `${prefix}${snippet}${suffix}`;
  result_link.append(title_element, snippet_element);
  result_link.addEventListener("click", () => set_sidebar(false));
  return result_link;
}

function update_search() {
  if (!search_input || !search_results) {
    return;
  }
  const query = normalize_search_text(search_input.value);
  search_results.replaceChildren();
  if (query.length < 2) {
    search_results.dataset.visible = "false";
    return;
  }

  const matches = search_documents.filter((entry) => entry.search_text.includes(query)).slice(0, 24);
  if (matches.length === 0) {
    const empty_result = document.createElement("p");
    empty_result.className = "search_result";
    empty_result.textContent = "本期没有匹配章节";
    search_results.append(empty_result);
  } else {
    for (const match of matches) {
      search_results.append(create_search_result(match, query));
    }
  }
  search_results.dataset.visible = "true";
}

search_input?.addEventListener("input", update_search);
search_input?.addEventListener("keydown", (event) => {
  if (event.key === "Escape") {
    search_input.value = "";
    update_search();
    search_input.blur();
  }
});

function update_progress() {
  if (!progress_bar) {
    return;
  }
  const scrollable_height = document.documentElement.scrollHeight - window.innerHeight;
  const progress = scrollable_height > 0 ? Math.min(1, window.scrollY / scrollable_height) : 0;
  progress_bar.style.width = `${(progress * 100).toFixed(2)}%`;
}

function set_active_chapter(chapter_id) {
  for (const toc_link of toc_links) {
    const is_active = toc_link.getAttribute("href") === `#${chapter_id}`;
    if (is_active) {
      toc_link.setAttribute("aria-current", "true");
      toc_link.closest("details")?.setAttribute("open", "");
    } else {
      toc_link.removeAttribute("aria-current");
    }
  }
  const active_chapter = chapter_elements.find((chapter) => chapter.id === chapter_id);
  if (toolbar_title && active_chapter) {
    toolbar_title.textContent = active_chapter.dataset.title || document.title;
  }
}

const chapter_observer = new IntersectionObserver((entries) => {
  const visible_entries = entries
    .filter((entry) => entry.isIntersecting)
    .sort((left, right) => right.intersectionRatio - left.intersectionRatio);
  if (visible_entries[0]) {
    set_active_chapter(visible_entries[0].target.id);
  }
}, {
  rootMargin: "-18% 0px -67% 0px",
  threshold: [0, 0.05, 0.2]
});

for (const chapter of chapter_elements) {
  chapter_observer.observe(chapter);
}

for (const toc_link of toc_links) {
  toc_link.addEventListener("click", () => set_sidebar(false));
}

for (const toc_section_link of toc_section_links) {
  toc_section_link.addEventListener("click", () => set_sidebar(false));
}

let progress_frame = 0;
window.addEventListener("scroll", () => {
  if (progress_frame) {
    return;
  }
  progress_frame = window.requestAnimationFrame(() => {
    update_progress();
    progress_frame = 0;
  });
}, { passive: true });

document.addEventListener("keydown", (event) => {
  if ((event.ctrlKey || event.metaKey) && event.key.toLocaleLowerCase() === "k") {
    event.preventDefault();
    search_input?.focus();
  }
  if (event.key === "Escape") {
    if (close_mermaid_viewer()) {
      event.preventDefault();
      return;
    }
    set_sidebar(false);
  }
});

function clamp(value, minimum, maximum) {
  return Math.min(maximum, Math.max(minimum, value));
}

function svg_dimensions(svg) {
  const view_box = (svg.getAttribute("viewBox") || "")
    .trim()
    .split(/\s+/)
    .map(Number);
  if (view_box.length === 4 && view_box.every(Number.isFinite) && view_box[2] > 0 && view_box[3] > 0) {
    return { width: view_box[2], height: view_box[3] };
  }
  const width = Number.parseFloat(svg.getAttribute("width") || "");
  const height = Number.parseFloat(svg.getAttribute("height") || "");
  return width > 0 && height > 0 ? { width, height } : null;
}

function normalize_mermaid_svg(svg) {
  const dimensions = svg_dimensions(svg);
  if (dimensions) {
    svg.setAttribute("width", String(Math.ceil(dimensions.width)));
    svg.setAttribute("height", String(Math.ceil(dimensions.height)));
    svg.setAttribute("preserveAspectRatio", "xMidYMid meet");
  }
  svg.style.removeProperty("width");
  svg.style.removeProperty("height");
  svg.style.removeProperty("max-width");
  svg.style.backgroundColor = "transparent";
}

function fit_svg_view_box_to_content(svg) {
  try {
    const bounds = svg.getBBox();
    if (!bounds.width || !bounds.height) {
      return;
    }
    const padding = Math.max(12, Math.min(32, Math.max(bounds.width, bounds.height) * 0.025));
    const width = Math.ceil(bounds.width + padding * 2);
    const height = Math.ceil(bounds.height + padding * 2);
    svg.setAttribute("viewBox", `${bounds.x - padding} ${bounds.y - padding} ${width} ${height}`);
    svg.setAttribute("width", String(width));
    svg.setAttribute("height", String(height));
    svg.setAttribute("preserveAspectRatio", "xMidYMid meet");
  } catch {
    // 浏览器无法测量尚未完成布局的 SVG 时保留 Mermaid 原始边界。
  }
}

function namespace_mermaid_svg_ids(svg, prefix) {
  const replacements = new Map();
  for (const element of svg.querySelectorAll("[id]")) {
    const old_id = element.id;
    const new_id = `${prefix}-${old_id}`;
    replacements.set(old_id, new_id);
    element.id = new_id;
  }
  for (const element of svg.querySelectorAll("*")) {
    for (const attribute of [...element.attributes]) {
      let value = attribute.value;
      for (const [old_id, new_id] of replacements) {
        value = value
          .replaceAll(`url(#${old_id})`, `url(#${new_id})`)
          .replaceAll(`#${old_id}`, `#${new_id}`);
      }
      if (value !== attribute.value) {
        element.setAttribute(attribute.name, value);
      }
    }
  }
  for (const style of svg.querySelectorAll("style")) {
    let value = style.textContent || "";
    for (const [old_id, new_id] of replacements) {
      value = value.replaceAll(`#${old_id}`, `#${new_id}`);
    }
    style.textContent = value;
  }
}

function create_mermaid_viewer() {
  const viewer = document.createElement("div");
  viewer.className = "mermaid_viewer";
  viewer.hidden = true;
  viewer.setAttribute("role", "dialog");
  viewer.setAttribute("aria-modal", "true");
  viewer.setAttribute("aria-labelledby", "mermaid_viewer_title");
  viewer.innerHTML = `
    <h2 class="mermaid_viewer_title" id="mermaid_viewer_title">Mermaid 图表大图</h2>
    <div class="mermaid_view_controls" aria-label="图表缩放控制">
      <button type="button" data_mermaid_action="zoom_out" title="缩小">−</button>
      <output data_mermaid_zoom aria-live="polite">100%</output>
      <button type="button" data_mermaid_action="zoom_in" title="放大">＋</button>
      <button type="button" data_mermaid_action="fit">适应屏幕</button>
    </div>
    <button type="button" class="mermaid_viewer_close" data_mermaid_action="close"><span aria-hidden="true">×</span> 退出大图</button>
    <div class="mermaid_viewer_canvas" data_mermaid_canvas tabindex="0" aria-label="可缩放和拖动的 Mermaid 图表">
      <div class="mermaid_viewer_positioner">
        <div class="mermaid_viewer_content" data_mermaid_viewer_content></div>
      </div>
    </div>
    <p class="mermaid_viewer_hint">Ctrl + 滚轮缩放 · 按住左键拖动 · 双击适屏 · Esc 退出</p>
  `;
  document.body.append(viewer);

  const canvas = viewer.querySelector("[data_mermaid_canvas]");
  const state = {
    scale: 1,
    x: 0,
    y: 0,
    dragging: false,
    pointer_id: null,
    drag_start_x: 0,
    drag_start_y: 0,
    drag_origin_x: 0,
    drag_origin_y: 0
  };
  const render_state = () => {
    canvas.style.setProperty("--mermaid_scale", String(state.scale));
    canvas.style.setProperty("--mermaid_pan_x", `${state.x}px`);
    canvas.style.setProperty("--mermaid_pan_y", `${state.y}px`);
    viewer.classList.toggle("is_dragging", state.dragging);
    viewer.querySelector("[data_mermaid_zoom]").textContent = `${Math.round(state.scale * 100)}%`;
  };
  const change_zoom = (factor) => {
    state.scale = clamp(state.scale * factor, mermaid_min_zoom, mermaid_max_zoom);
    render_state();
  };
  const fit_to_viewport = () => {
    const svg = canvas.querySelector("svg");
    const dimensions = svg ? svg_dimensions(svg) : null;
    if (!dimensions) {
      return;
    }
    const bounds = canvas.getBoundingClientRect();
    if (!bounds.width || !bounds.height) {
      return;
    }
    state.scale = clamp(
      Math.min(bounds.width / dimensions.width, bounds.height / dimensions.height) * 0.88,
      mermaid_min_zoom,
      mermaid_max_zoom
    );
    state.x = 0;
    state.y = 0;
    render_state();
  };

  viewer.querySelector("[data_mermaid_action='zoom_out']").addEventListener("click", () => change_zoom(0.8));
  viewer.querySelector("[data_mermaid_action='zoom_in']").addEventListener("click", () => change_zoom(1.25));
  viewer.querySelector("[data_mermaid_action='fit']").addEventListener("click", fit_to_viewport);
  viewer.querySelector("[data_mermaid_action='close']").addEventListener("click", close_mermaid_viewer);
  canvas.addEventListener("dblclick", fit_to_viewport);
  canvas.addEventListener("wheel", (event) => {
    if (!event.ctrlKey) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    const bounds = canvas.getBoundingClientRect();
    const pointer_x = event.clientX - bounds.left - bounds.width / 2;
    const pointer_y = event.clientY - bounds.top - bounds.height / 2;
    const next_scale = clamp(
      state.scale * Math.exp(-event.deltaY * 0.002),
      mermaid_min_zoom,
      mermaid_max_zoom
    );
    const ratio = next_scale / state.scale;
    state.x = pointer_x - (pointer_x - state.x) * ratio;
    state.y = pointer_y - (pointer_y - state.y) * ratio;
    state.scale = next_scale;
    render_state();
  }, { passive: false });
  canvas.addEventListener("pointerdown", (event) => {
    if (event.button !== 0) {
      return;
    }
    state.pointer_id = event.pointerId;
    state.drag_start_x = event.clientX;
    state.drag_start_y = event.clientY;
    state.drag_origin_x = state.x;
    state.drag_origin_y = state.y;
    state.dragging = true;
    canvas.setPointerCapture?.(event.pointerId);
    render_state();
  });
  canvas.addEventListener("pointermove", (event) => {
    if (!state.dragging || event.pointerId !== state.pointer_id) {
      return;
    }
    state.x = state.drag_origin_x + event.clientX - state.drag_start_x;
    state.y = state.drag_origin_y + event.clientY - state.drag_start_y;
    render_state();
  });
  const finish_drag = (event) => {
    if (event.pointerId !== state.pointer_id) {
      return;
    }
    if (canvas.hasPointerCapture?.(event.pointerId)) {
      canvas.releasePointerCapture(event.pointerId);
    }
    state.dragging = false;
    state.pointer_id = null;
    render_state();
  };
  canvas.addEventListener("pointerup", finish_drag);
  canvas.addEventListener("pointercancel", finish_drag);

  return { viewer, canvas, state, render_state, fit_to_viewport };
}

function open_mermaid_viewer(source_svg, trigger) {
  if (!mermaid_viewer) {
    mermaid_viewer = create_mermaid_viewer();
  }
  const clone = source_svg.cloneNode(true);
  namespace_mermaid_svg_ids(clone, `markbook-mermaid-viewer-${++mermaid_viewer_sequence}`);
  normalize_mermaid_svg(clone);
  fit_svg_view_box_to_content(clone);
  mermaid_viewer.viewer.querySelector("[data_mermaid_viewer_content]").replaceChildren(clone);
  mermaid_viewer.state.scale = 1;
  mermaid_viewer.state.x = 0;
  mermaid_viewer.state.y = 0;
  mermaid_viewer.state.dragging = false;
  mermaid_viewer.state.pointer_id = null;
  mermaid_viewer.render_state();
  mermaid_viewer_trigger = trigger;
  mermaid_viewer.viewer.hidden = false;
  page_body.dataset.mermaid_viewer_open = "true";
  mermaid_viewer.canvas.focus({ preventScroll: true });
  window.requestAnimationFrame(mermaid_viewer.fit_to_viewport);
  mermaid_viewer_resize_observer?.disconnect();
  if (window.ResizeObserver) {
    mermaid_viewer_resize_observer = new ResizeObserver(mermaid_viewer.fit_to_viewport);
    mermaid_viewer_resize_observer.observe(mermaid_viewer.canvas);
  }
}

function close_mermaid_viewer() {
  if (!mermaid_viewer || mermaid_viewer.viewer.hidden) {
    return false;
  }
  mermaid_viewer_resize_observer?.disconnect();
  mermaid_viewer_resize_observer = null;
  mermaid_viewer.viewer.hidden = true;
  mermaid_viewer.viewer.querySelector("[data_mermaid_viewer_content]").replaceChildren();
  page_body.dataset.mermaid_viewer_open = "false";
  mermaid_viewer_trigger?.focus({ preventScroll: true });
  mermaid_viewer_trigger = null;
  return true;
}

function enhance_mermaid_block(block) {
  if (block.closest(".mermaid_diagram")) {
    return;
  }
  const svg = block.querySelector("svg");
  if (!svg || !block.parentNode) {
    return;
  }
  normalize_mermaid_svg(svg);
  fit_svg_view_box_to_content(svg);

  const figure = document.createElement("figure");
  figure.className = "mermaid_diagram";
  const button = document.createElement("button");
  button.type = "button";
  button.className = "mermaid_fullscreen_button";
  button.title = "打开 Mermaid 图表大图查看器";
  button.innerHTML = '<span aria-hidden="true">⛶</span> 大图查看';
  const canvas = document.createElement("div");
  canvas.className = "mermaid_canvas";
  const content = document.createElement("div");
  content.className = "mermaid_content";
  block.parentNode.insertBefore(figure, block);
  content.append(block);
  canvas.append(content);
  figure.append(button, canvas);
  button.addEventListener("click", () => open_mermaid_viewer(svg, button));
}

async function render_mermaid() {
  const mermaid_blocks = [...document.querySelectorAll("pre.mermaid")];
  if (!mermaid_blocks.length || !window.mermaid) {
    return;
  }
  window.mermaid.initialize({
    startOnLoad: false,
    securityLevel: "strict",
    theme: "neutral",
    fontFamily: "Noto Sans SC, Source Han Sans SC, Microsoft YaHei, sans-serif",
    flowchart: { htmlLabels: false, useMaxWidth: false },
    sequence: { useMaxWidth: false, wrap: true }
  });
  try {
    await window.mermaid.run({ nodes: mermaid_blocks });
    for (const block of mermaid_blocks) {
      enhance_mermaid_block(block);
    }
  } catch (error) {
    console.error("Mermaid rendering failed", error);
  }
}

update_progress();
render_mermaid();
