import { Registry, INITIAL, parseRawGrammar, type IGrammar, type StateStack } from "vscode-textmate";
import { loadWASM, OnigScanner, OnigString } from "vscode-oniguruma";
import oniguruma_wasm from "vscode-oniguruma/release/onig.wasm";
import c_grammar from "../vendor/vscode_cpp/syntaxes/c.tmLanguage.json";
import cpp_grammar from "../vendor/vscode_cpp/syntaxes/cpp.tmLanguage.json";
import cpp_macro_grammar from "../vendor/vscode_cpp/syntaxes/cpp.embedded.macro.tmLanguage.json";
import platform_grammar from "../vendor/vscode_cpp/syntaxes/platform.tmLanguage.json";
import extension_css from "./typora_enhancements.css";

type code_mirror_stream = {
  string: string;
  pos: number;
  sol(): boolean;
  eol(): boolean;
  skipToEnd(): void;
};

type code_mirror_instance = {
  state: Record<string, unknown>;
  getOption(name: string): unknown;
  setOption(name: string, value: unknown): void;
  refresh(): void;
};

type code_mirror_constructor = {
  defineMode(name: string, factory: () => unknown): void;
};

type textmate_state = {
  rule_stack: StateStack;
  pending_rule_stack: StateStack;
  line: string;
  tokens: Array<{ startIndex: number; endIndex: number; scopes: string[] }>;
  token_index: number;
};

declare global {
  interface Window {
    CodeMirror?: code_mirror_constructor;
  }
}

const EXTENSION_STYLE_ID = "linux-note-typora-enhancements-style";
const C_MODE_NAME = "linux-note-vscode-textmate-c";
const CPP_MODE_NAME = "linux-note-vscode-textmate-cpp";
const MINIMUM_ZOOM = 0.2;
const MAXIMUM_ZOOM = 6;
const ZOOM_FACTOR = 1.25;
const MINIMUM_COLLAPSED_CODE_HEIGHT = 320;
const MAXIMUM_COLLAPSED_CODE_HEIGHT = 560;
const CODE_COLLAPSE_TOLERANCE = 48;

let c_textmate_grammar: IGrammar | null = null;
let cpp_textmate_grammar: IGrammar | null = null;
let scan_timer = 0;
const mermaid_buttons = new Map<Element, HTMLButtonElement>();

function ensure_style(): void {
  if (document.getElementById(EXTENSION_STYLE_ID)) return;
  const style = document.createElement("style");
  style.id = EXTENSION_STYLE_ID;
  style.textContent = extension_css;
  document.head.append(style);
}

function raw_grammar(value: unknown, path: string) {
  return parseRawGrammar(JSON.stringify(value), path);
}

async function load_textmate_grammars(): Promise<void> {
  await loadWASM(oniguruma_wasm.buffer);
  const grammar_sources = new Map([
    ["source.c", raw_grammar(c_grammar, "c.tmLanguage.json")],
    ["source.cpp", raw_grammar(cpp_grammar, "cpp.tmLanguage.json")],
    ["source.cpp.embedded.macro", raw_grammar(cpp_macro_grammar, "cpp.embedded.macro.tmLanguage.json")],
    ["source.c.platform", raw_grammar(platform_grammar, "platform.tmLanguage.json")],
  ]);
  const registry = new Registry({
    onigLib: Promise.resolve({
      createOnigScanner: (sources: string[]) => new OnigScanner(sources),
      createOnigString: (value: string) => new OnigString(value),
    }),
    loadGrammar: async (scope_name: string) => grammar_sources.get(scope_name) ?? null,
  });
  c_textmate_grammar = await registry.loadGrammar("source.c");
  cpp_textmate_grammar = await registry.loadGrammar("source.cpp");
  if (!c_textmate_grammar || !cpp_textmate_grammar) throw new Error("C/C++ TextMate grammar failed to load");
}

function scope_style(scopes: string[]): string {
  const joined = scopes.join(" ");
  if (/\binvalid(?:\.|\b)/u.test(joined)) return "tm-invalid";
  if (/\bcomment(?:\.|\b)/u.test(joined)) return "tm-comment";
  if (/\bstring(?:\.|\b)/u.test(joined)) return "tm-string";
  if (/\bconstant\.numeric(?:\.|\b)/u.test(joined)) return "tm-number";
  if (/\bmeta\.preprocessor(?:\.|\b)|\bkeyword\.control\.directive(?:\.|\b)|\bentity\.name\.function\.preprocessor(?:\.|\b)/u.test(joined)) return "tm-preprocessor";
  if (/\bentity\.name\.function(?:\.|\b)|\bsupport\.function(?:\.|\b)|\bentity\.name\.operator(?:\.|\b)/u.test(joined)) return "tm-function";
  if (/\bvariable\.parameter(?:\.|\b)/u.test(joined)) return "tm-parameter";
  if (/\bvariable\.other\.property(?:\.|\b)|\bvariable\.object\.property(?:\.|\b)/u.test(joined)) return "tm-property";
  if (/\bentity\.name\.namespace(?:\.|\b)|\bentity\.name\.scope-resolution(?:\.|\b)/u.test(joined)) return "tm-namespace";
  if (/\bsupport\.type(?:\.|\b)|\bsupport\.class(?:\.|\b)|\bentity\.name\.type(?:\.|\b)|\bentity\.name\.class(?:\.|\b)/u.test(joined)) return "tm-type";
  if (/\bentity\.other\.attribute(?:\.|\b)/u.test(joined)) return "tm-attribute";
  if (/\bkeyword\.control(?:\.|\b)|\bkeyword\.other\.(?:using|operator)(?:\.|\b)/u.test(joined)) return "tm-control";
  if (/\bstorage(?:\.|\b)|\bkeyword(?:\.|\b)/u.test(joined)) return "tm-keyword";
  if (/\bvariable(?:\.|\b)|\bmeta\.definition\.variable\.name(?:\.|\b)|\bentity\.name\.variable(?:\.|\b)|\bsupport\.variable(?:\.|\b)/u.test(joined)) return "tm-variable";
  if (/\bconstant(?:\.|\b)/u.test(joined)) return "tm-constant";
  if (/\bkeyword\.operator(?:\.|\b)/u.test(joined)) return "tm-operator";
  if (/\bpunctuation(?:\.|\b)/u.test(joined)) return "tm-punctuation";
  return "tm-plain";
}

function create_textmate_mode(grammar: IGrammar) {
  return {
    startState(): textmate_state {
      return {
        rule_stack: INITIAL,
        pending_rule_stack: INITIAL,
        line: "",
        tokens: [],
        token_index: 0,
      };
    },
    copyState(state: textmate_state): textmate_state {
      return {
        rule_stack: state.rule_stack,
        pending_rule_stack: state.pending_rule_stack,
        line: state.line,
        tokens: state.tokens,
        token_index: state.token_index,
      };
    },
    blankLine(state: textmate_state): void {
      state.rule_stack = grammar.tokenizeLine("", state.rule_stack).ruleStack;
      state.pending_rule_stack = state.rule_stack;
      state.line = "";
      state.tokens = [];
      state.token_index = 0;
    },
    token(stream: code_mirror_stream, state: textmate_state): string {
      if (stream.sol() || state.line !== stream.string) {
        const result = grammar.tokenizeLine(stream.string, state.rule_stack);
        state.line = stream.string;
        state.tokens = result.tokens;
        state.token_index = 0;
        state.pending_rule_stack = result.ruleStack;
      }
      while (state.token_index < state.tokens.length
          && (state.tokens[state.token_index]?.endIndex ?? 0) <= stream.pos) {
        state.token_index += 1;
      }
      const token = state.tokens[state.token_index];
      if (!token) {
        stream.skipToEnd();
        state.rule_stack = state.pending_rule_stack;
        return "tm-plain";
      }
      stream.pos = Math.min(stream.string.length, Math.max(stream.pos + 1, token.endIndex));
      if (stream.eol()) state.rule_stack = state.pending_rule_stack;
      return scope_style(token.scopes);
    },
  };
}

function normalize_language(value: string): "c" | "cpp" | null {
  const language = value.trim().toLowerCase();
  if (["c", "clike", "csrc", "text/x-csrc"].includes(language)) return "c";
  if (["c++", "cpp", "cc", "cxx", "h", "hpp", "h++", "text/x-c++src"].includes(language)) return "cpp";
  return null;
}

function code_mirror_for_fence(fence: Element): code_mirror_instance | null {
  const wrapper = fence.querySelector<HTMLElement>(".CodeMirror");
  const value = wrapper && (wrapper as HTMLElement & { CodeMirror?: code_mirror_instance }).CodeMirror;
  return value ?? null;
}

function apply_textmate_mode(fence: Element): void {
  const language = normalize_language(fence.getAttribute("lang") ?? "");
  if (!language) return;
  const code_mirror = code_mirror_for_fence(fence);
  if (!code_mirror) return;
  const mode = language === "c" ? C_MODE_NAME : CPP_MODE_NAME;
  if (code_mirror.getOption("mode") === mode) return;
  code_mirror.setOption("mode", mode);
  code_mirror.state.linux_note_textmate_language = language;
  code_mirror.refresh();
}

function collapsed_code_height(): number {
  return Math.round(clamp(window.innerHeight * 0.52, MINIMUM_COLLAPSED_CODE_HEIGHT, MAXIMUM_COLLAPSED_CODE_HEIGHT));
}

function code_fence_is_diagram(fence: Element): boolean {
  const language = (fence.getAttribute("lang") ?? "").trim().toLowerCase();
  return ["flow", "flowchart", "mermaid", "plantuml", "sequence"].includes(language)
    || Boolean(fence.querySelector(".md-diagram-panel-preview"));
}

function remove_code_collapse(fence: HTMLElement): void {
  fence.classList.remove("linux-note-code-collapsible", "is-code-collapsed", "is-code-expanded");
  fence.style.removeProperty("--linux-note-code-collapsed-height");
  fence.querySelector(":scope > .linux-note-code-toolbar")?.remove();
}

function render_code_toggle(button: HTMLButtonElement, expanded: boolean): void {
  button.setAttribute("aria-expanded", String(expanded));
  button.innerHTML = expanded
    ? '<span aria-hidden="true">↥</span><span>收起代码</span>'
    : '<span aria-hidden="true">↧</span><span>展开全部代码</span>';
  button.title = expanded ? "恢复长代码块的限高显示" : "展示这个代码块的全部内容";
}

function set_code_expanded(fence: HTMLElement, button: HTMLButtonElement, expanded: boolean): void {
  fence.classList.toggle("is-code-expanded", expanded);
  fence.classList.toggle("is-code-collapsed", !expanded);
  render_code_toggle(button, expanded);
  if (!expanded) {
    const scroller = fence.querySelector<HTMLElement>(".CodeMirror-scroll");
    if (scroller) scroller.scrollTop = 0;
  }
  requestAnimationFrame(() => code_mirror_for_fence(fence)?.refresh());
}

function ensure_code_collapse(fence_element: Element): void {
  if (!(fence_element instanceof HTMLElement)) return;
  const fence = fence_element;
  const scroller = fence.querySelector<HTMLElement>(".CodeMirror-scroll");
  const sizer = fence.querySelector<HTMLElement>(".CodeMirror-sizer");
  if (!scroller || code_fence_is_diagram(fence)) {
    remove_code_collapse(fence);
    return;
  }

  const maximum_height = collapsed_code_height();
  const content_height = Math.max(scroller.scrollHeight, sizer?.scrollHeight ?? 0, sizer?.offsetHeight ?? 0);
  if (content_height <= maximum_height + CODE_COLLAPSE_TOLERANCE) {
    remove_code_collapse(fence);
    return;
  }

  fence.classList.add("linux-note-code-collapsible");
  fence.style.setProperty("--linux-note-code-collapsed-height", `${maximum_height}px`);
  let toolbar = fence.querySelector<HTMLElement>(":scope > .linux-note-code-toolbar");
  let button = toolbar?.querySelector<HTMLButtonElement>(".linux-note-code-toggle");
  if (!toolbar || !button) {
    toolbar?.remove();
    toolbar = document.createElement("div");
    toolbar.className = "linux-note-code-toolbar";
    toolbar.contentEditable = "false";
    button = document.createElement("button");
    button.type = "button";
    button.className = "linux-note-code-toggle";
    button.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      set_code_expanded(fence, button!, !fence.classList.contains("is-code-expanded"));
    });
    toolbar.append(button);
    fence.append(toolbar);
  }
  if (!fence.classList.contains("is-code-collapsed") && !fence.classList.contains("is-code-expanded")) {
    set_code_expanded(fence, button, false);
  } else {
    render_code_toggle(button, fence.classList.contains("is-code-expanded"));
  }
}

function schedule_scan(): void {
  window.clearTimeout(scan_timer);
  scan_timer = window.setTimeout(scan_document, 80);
}

function scan_document(): void {
  document.querySelectorAll(".md-fences[lang]").forEach(apply_textmate_mode);
  document.querySelectorAll(".md-fences").forEach(ensure_code_collapse);
  const diagram_containers = new Set<Element>();
  document.querySelectorAll(".md-diagram-panel-preview").forEach((preview) => {
    diagram_containers.add(mermaid_container_for_preview(preview));
  });
  diagram_containers.forEach(ensure_mermaid_button);
  for (const [container, button] of mermaid_buttons) {
    if (!container.isConnected) {
      mermaid_buttons.delete(container);
    } else if (!button.isConnected) {
      mermaid_buttons.delete(container);
      ensure_mermaid_button(container);
    }
  }
}

function namespace_svg_ids(svg: SVGSVGElement): void {
  const prefix = `linux-note-mermaid-${Date.now().toString(36)}`;
  const replacements = new Map<string, string>();
  svg.querySelectorAll<SVGElement>("[id]").forEach((element) => {
    const old_id = element.id;
    const new_id = `${prefix}-${old_id}`;
    replacements.set(old_id, new_id);
    element.id = new_id;
  });
  svg.querySelectorAll<SVGElement>("*").forEach((element) => {
    for (const attribute of Array.from(element.attributes)) {
      let value = attribute.value;
      for (const [old_id, new_id] of replacements) {
        value = value.replaceAll(`url(#${old_id})`, `url(#${new_id})`).replaceAll(`#${old_id}`, `#${new_id}`);
      }
      if (value !== attribute.value) element.setAttribute(attribute.name, value);
    }
  });
  svg.querySelectorAll("style").forEach((style) => {
    let value = style.textContent ?? "";
    for (const [old_id, new_id] of replacements) value = value.replaceAll(`#${old_id}`, `#${new_id}`);
    style.textContent = value;
  });
}

function clone_mermaid_svg(preview: Element): SVGSVGElement | null {
  const source = preview.querySelector("svg");
  if (!(source instanceof SVGSVGElement)) return null;
  const svg = source.cloneNode(true) as SVGSVGElement;
  namespace_svg_ids(svg);
  try {
    const bounds = source.getBBox();
    if (!bounds.width || !bounds.height) throw new Error("empty SVG bounds");
    const padding = Math.max(12, Math.min(32, Math.max(bounds.width, bounds.height) * 0.025));
    const width = Math.ceil(bounds.width + padding * 2);
    const height = Math.ceil(bounds.height + padding * 2);
    svg.setAttribute("viewBox", [bounds.x - padding, bounds.y - padding, width, height].join(" "));
    svg.setAttribute("width", String(width));
    svg.setAttribute("height", String(height));
  } catch {
    const view_box = svg.getAttribute("viewBox")?.trim().split(/\s+/u).map(Number);
    if (view_box?.length === 4 && view_box.every(Number.isFinite)) {
      svg.setAttribute("width", String(Math.max(1, Math.ceil(view_box[2] ?? 1))));
      svg.setAttribute("height", String(Math.max(1, Math.ceil(view_box[3] ?? 1))));
    } else {
      const bounds = source.getBoundingClientRect();
      svg.setAttribute("width", String(Math.max(1, Math.ceil(bounds.width))));
      svg.setAttribute("height", String(Math.max(1, Math.ceil(bounds.height))));
    }
  }
  svg.removeAttribute("style");
  svg.style.backgroundColor = "transparent";
  svg.setAttribute("preserveAspectRatio", "xMidYMid meet");
  return svg;
}

function clamp(value: number, minimum: number, maximum: number): number {
  return Math.min(maximum, Math.max(minimum, value));
}

function open_mermaid_viewer(preview: Element): void {
  const svg = clone_mermaid_svg(preview);
  if (!svg) return;
  document.querySelector(".linux-note-mermaid-viewer")?.remove();
  const previous_focus = document.activeElement instanceof HTMLElement ? document.activeElement : null;
  const viewer = document.createElement("div");
  viewer.className = "linux-note-mermaid-viewer";
  viewer.setAttribute("role", "dialog");
  viewer.setAttribute("aria-modal", "true");
  viewer.setAttribute("aria-label", "Mermaid 图表全屏查看");
  viewer.innerHTML = `
    <div class="linux-note-mermaid-toolbar" aria-label="图表缩放控制">
      <button type="button" data-action="zoom-out" title="缩小">−</button>
      <output>100%</output>
      <button type="button" data-action="zoom-in" title="放大">＋</button>
      <button type="button" data-action="fit-width">适应宽度</button>
      <button type="button" data-action="fit">适应屏幕</button>
      <button type="button" data-action="reset">100%</button>
    </div>
    <button type="button" class="linux-note-mermaid-close" data-action="close"><span aria-hidden="true">×</span> 退出全屏</button>
    <div class="linux-note-mermaid-canvas">
      <div class="linux-note-mermaid-positioner"><div class="linux-note-mermaid-content"></div></div>
    </div>
    <div class="linux-note-mermaid-hint">Ctrl + 滚轮缩放 · 按住左键拖动 · Esc 退出</div>`;
  const canvas = viewer.querySelector<HTMLElement>(".linux-note-mermaid-canvas");
  const content = viewer.querySelector<HTMLElement>(".linux-note-mermaid-content");
  const output = viewer.querySelector<HTMLOutputElement>("output");
  if (!canvas || !content || !output) return;
  content.append(svg);
  document.body.append(viewer);
  document.body.classList.add("linux-note-mermaid-viewer-open");

  const view = { scale: 1, x: 0, y: 0 };
  const drag = { active: false, pointer_id: 0, start_x: 0, start_y: 0, origin_x: 0, origin_y: 0 };
  const apply_view = () => {
    viewer.style.setProperty("--linux-note-mermaid-scale", String(view.scale));
    viewer.style.setProperty("--linux-note-mermaid-pan-x", `${view.x}px`);
    viewer.style.setProperty("--linux-note-mermaid-pan-y", `${view.y}px`);
    output.value = `${Math.round(view.scale * 100)}%`;
    output.textContent = output.value;
  };
  const set_zoom = (scale: number, pointer_x = 0, pointer_y = 0) => {
    const next_scale = clamp(scale, MINIMUM_ZOOM, MAXIMUM_ZOOM);
    const ratio = next_scale / view.scale;
    view.x = pointer_x - (pointer_x - view.x) * ratio;
    view.y = pointer_y - (pointer_y - view.y) * ratio;
    view.scale = next_scale;
    apply_view();
  };
  const fit = () => {
    const view_box = svg.viewBox.baseVal;
    const width = view_box.width || Number(svg.getAttribute("width"));
    const height = view_box.height || Number(svg.getAttribute("height"));
    const bounds = canvas.getBoundingClientRect();
    if (!width || !height || !bounds.width || !bounds.height) return;
    view.scale = clamp(Math.min(bounds.width / width, bounds.height / height) * 0.88, MINIMUM_ZOOM, MAXIMUM_ZOOM);
    view.x = 0;
    view.y = 0;
    apply_view();
  };
  const fit_width = () => {
    const view_box = svg.viewBox.baseVal;
    const width = view_box.width || Number(svg.getAttribute("width"));
    const bounds = canvas.getBoundingClientRect();
    if (!width || !bounds.width) return;
    view.scale = clamp((bounds.width / width) * 0.92, MINIMUM_ZOOM, MAXIMUM_ZOOM);
    view.x = 0;
    view.y = 0;
    apply_view();
  };
  const reset = () => {
    view.scale = 1;
    view.x = 0;
    view.y = 0;
    apply_view();
  };
  const close = () => {
    window.removeEventListener("keydown", handle_keydown, true);
    viewer.remove();
    document.body.classList.remove("linux-note-mermaid-viewer-open");
    previous_focus?.focus();
  };
  const handle_keydown = (event: KeyboardEvent) => {
    if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      close();
    } else if (event.ctrlKey && (event.key === "+" || event.key === "=")) {
      event.preventDefault();
      set_zoom(view.scale * ZOOM_FACTOR);
    } else if (event.ctrlKey && event.key === "-") {
      event.preventDefault();
      set_zoom(view.scale / ZOOM_FACTOR);
    } else if (event.ctrlKey && event.key === "0") {
      event.preventDefault();
      reset();
    }
  };
  viewer.addEventListener("click", (event) => {
    const button = (event.target as Element).closest<HTMLButtonElement>("button[data-action]");
    if (!button) return;
    const action = button.dataset.action;
    if (action === "close") close();
    else if (action === "zoom-out") set_zoom(view.scale / ZOOM_FACTOR);
    else if (action === "zoom-in") set_zoom(view.scale * ZOOM_FACTOR);
    else if (action === "fit-width") fit_width();
    else if (action === "fit") fit();
    else if (action === "reset") reset();
  });
  canvas.addEventListener("wheel", (event) => {
    if (!event.ctrlKey) return;
    event.preventDefault();
    event.stopPropagation();
    const bounds = canvas.getBoundingClientRect();
    const pointer_x = event.clientX - bounds.left - bounds.width / 2;
    const pointer_y = event.clientY - bounds.top - bounds.height / 2;
    set_zoom(view.scale * Math.exp(-event.deltaY * 0.002), pointer_x, pointer_y);
  }, { passive: false });
  canvas.addEventListener("pointerdown", (event) => {
    if (event.button !== 0) return;
    drag.active = true;
    drag.pointer_id = event.pointerId;
    drag.start_x = event.clientX;
    drag.start_y = event.clientY;
    drag.origin_x = view.x;
    drag.origin_y = view.y;
    canvas.setPointerCapture(event.pointerId);
    viewer.classList.add("is-dragging");
  });
  canvas.addEventListener("pointermove", (event) => {
    if (!drag.active || event.pointerId !== drag.pointer_id) return;
    view.x = drag.origin_x + event.clientX - drag.start_x;
    view.y = drag.origin_y + event.clientY - drag.start_y;
    apply_view();
  });
  const end_drag = (event: PointerEvent) => {
    if (event.pointerId !== drag.pointer_id) return;
    drag.active = false;
    if (canvas.hasPointerCapture(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
    viewer.classList.remove("is-dragging");
  };
  canvas.addEventListener("pointerup", end_drag);
  canvas.addEventListener("pointercancel", end_drag);
  canvas.addEventListener("dblclick", fit);
  window.addEventListener("keydown", handle_keydown, true);
  requestAnimationFrame(() => {
    reset();
    viewer.querySelector<HTMLButtonElement>(".linux-note-mermaid-close")?.focus();
  });
}

function mermaid_container_for_preview(preview: Element): Element {
  return preview.closest(".md-fences") ?? preview.closest(".md-diagram-panel") ?? preview.parentElement ?? preview;
}

function select_mermaid_preview(container: Element): Element | null {
  const previews = Array.from(container.matches(".md-diagram-panel-preview")
    ? [container]
    : container.querySelectorAll(".md-diagram-panel-preview"));
  const with_svg = previews.filter((preview) => preview.querySelector("svg"));
  const candidates = with_svg.length ? with_svg : previews;
  let selected: Element | null = null;
  let selected_area = -1;
  for (const preview of candidates) {
    const style = getComputedStyle(preview);
    const bounds = preview.getBoundingClientRect();
    const visible = style.display !== "none" && style.visibility !== "hidden" && bounds.width > 0 && bounds.height > 0;
    const area = visible ? bounds.width * bounds.height : 0;
    if (area >= selected_area) {
      selected = preview;
      selected_area = area;
    }
  }
  return selected;
}

function ensure_mermaid_button(container: Element): void {
  const preview = select_mermaid_preview(container);
  if (!preview) return;
  const existing_button = mermaid_buttons.get(container);
  const existing_toolbar = existing_button?.closest(".linux-note-mermaid-inline-toolbar");
  const toolbars = Array.from(container.querySelectorAll(":scope .linux-note-mermaid-inline-toolbar"));
  if (existing_button?.isConnected
      && existing_toolbar?.parentElement === preview
      && toolbars.length === 1) return;
  toolbars.forEach((toolbar) => toolbar.remove());

  const toolbar = document.createElement("div");
  toolbar.className = "linux-note-mermaid-inline-toolbar";
  toolbar.contentEditable = "false";
  const button = document.createElement("button");
  button.type = "button";
  button.className = "linux-note-mermaid-open";
  button.title = "全屏查看 Mermaid 图表";
  button.innerHTML = '<span aria-hidden="true">⛶</span><span>全屏查看</span>';
  button.addEventListener("click", (event) => {
    event.preventDefault();
    event.stopPropagation();
    open_mermaid_viewer(preview);
  });
  toolbar.append(button);
  preview.prepend(toolbar);
  mermaid_buttons.set(container, button);
}

async function initialize(): Promise<void> {
  ensure_style();
  await load_textmate_grammars();
  if (!window.CodeMirror) throw new Error("Typora CodeMirror is unavailable");
  window.CodeMirror.defineMode(C_MODE_NAME, () => create_textmate_mode(c_textmate_grammar!));
  window.CodeMirror.defineMode(CPP_MODE_NAME, () => create_textmate_mode(cpp_textmate_grammar!));
  scan_document();
  new MutationObserver(schedule_scan).observe(document.body, {
    subtree: true,
    childList: true,
    attributes: true,
    attributeFilter: ["class", "hidden", "lang"],
  });
  window.addEventListener("resize", schedule_scan, { passive: true });
  document.documentElement.setAttribute("data-linux-note-typora-enhancements", "ready");
}

const initialization_state = document.documentElement.getAttribute("data-linux-note-typora-enhancements");
if (initialization_state !== "loading" && initialization_state !== "ready") {
  document.documentElement.setAttribute("data-linux-note-typora-enhancements", "loading");
  void initialize().catch((error: unknown) => {
    document.documentElement.setAttribute("data-linux-note-typora-enhancements", "failed");
    console.error("[linux-note Typora enhancements]", error);
  });
}
