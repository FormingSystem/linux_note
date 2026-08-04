import mermaid from "mermaid";
import { PREVIEW_PROTOCOL_VERSION } from "@loop/markdown-engine/contracts";
import {
  is_mermaid_connect_message,
  is_mermaid_render_message,
  type mermaid_frame_message,
  type mermaid_render_message,
} from "../preview/preview_frame_protocol.mts";
import { mermaid_source_has_document_configuration } from "../preview/mermaid_source_policy.mts";
import type { workbench_theme } from "../theme/workbench_theme.mts";

const SVG_NAMESPACE = "http://www.w3.org/2000/svg";
const MAXIMUM_MERMAID_SVG_BYTES = 4 * 1024 * 1024;
const MAXIMUM_MERMAID_SVG_ELEMENTS = 50_000;
const MAXIMUM_MERMAID_SVG_DEPTH = 64;
const MAXIMUM_MERMAID_SVG_ATTRIBUTE_LENGTH = 1 * 1024 * 1024;
const SAFE_SVG_ELEMENTS = new Set([
  "svg", "g", "path", "rect", "circle", "ellipse", "line", "polyline", "polygon", "text", "tspan",
  "marker", "defs", "symbol", "clippath", "mask", "lineargradient", "radialgradient", "stop", "title", "desc", "style", "use",
  "filter", "fedropshadow",
]);
const SAFE_SVG_ATTRIBUTES = new Set([
  "id", "class", "name", "viewbox", "width", "height", "x", "y", "x1", "y1", "x2", "y2", "cx", "cy", "r", "rx", "ry", "dx", "dy",
  "points", "d", "transform", "fill", "fill-opacity", "stroke", "stroke-width", "stroke-dasharray", "stroke-linecap",
  "stroke-linejoin", "stroke-opacity", "stroke-miterlimit", "stroke-dashoffset", "opacity", "font-family", "font-size", "font-style", "font-weight", "text-anchor",
  "dominant-baseline", "alignment-baseline", "baseline-shift", "letter-spacing", "word-spacing", "marker-start", "marker-mid", "marker-end",
  "refx", "refy", "markerwidth", "markerheight", "markerunits", "orient", "offset", "stop-color", "stop-opacity", "gradientunits", "gradienttransform", "spreadmethod", "clip-path",
  "clip-rule", "fill-rule", "mask", "preserveaspectratio", "pathlength", "startoffset", "vector-effect", "shape-rendering",
  "filter", "filterunits", "primitiveunits", "stddeviation", "flood-color", "flood-opacity", "in", "result",
  "text-rendering", "text-decoration", "pointer-events", "visibility", "display", "color", "role", "tabindex", "aria-label",
  "aria-labelledby", "xmlns", "xlink", "style", "href",
  "aria-roledescription",
]);
const UNSAFE_CSS_PATTERN = /(?:@import|expression\s*\(|javascript:|behavior\s*:|-moz-binding)/iu;
const LOCAL_REFERENCE_PATTERN = /^#[a-zA-Z0-9_.:-]{1,160}$/u;
const MINIMUM_ZOOM = 0.5;
const MAXIMUM_ZOOM = 3;
const ZOOM_STEP = 0.25;

let active_port: MessagePort | null = null;
let active_session_nonce: string | null = null;
let active_render: mermaid_render_message | null = null;
let active_svg: SVGSVGElement | null = null;
let current_zoom = 1;

function initialize_mermaid(theme: workbench_theme): void {
  const dark = theme === "dark";
  mermaid.initialize({
    startOnLoad: false,
    securityLevel: "strict",
    htmlLabels: false,
    theme: "base",
    darkMode: dark,
    deterministicIds: true,
    suppressErrorRendering: true,
    secure: [
      "secure", "securityLevel", "startOnLoad", "maxTextSize", "suppressErrorRendering", "maxEdges",
      "theme", "themeVariables", "themeCSS", "darkMode", "fontFamily", "fontSize", "logLevel", "htmlLabels", "deterministicIds",
      "deterministicIDSeed", "look", "layout", "handDrawnSeed", "flowchart", "sequence", "state", "class",
      "er", "gantt", "pie", "requirement", "journey", "timeline", "gitGraph", "c4", "sankey", "xyChart",
      "mindmap", "kanban", "architecture", "block", "packet", "radar", "treemap",
    ],
    flowchart: { useMaxWidth: true },
    themeVariables: dark ? {
      background: "#1e1e1e",
      primaryColor: "#252526",
      primaryTextColor: "#cccccc",
      primaryBorderColor: "#3794ff",
      secondaryColor: "#2a2d2e",
      secondaryTextColor: "#cccccc",
      secondaryBorderColor: "#4fc1ff",
      tertiaryColor: "#181818",
      tertiaryTextColor: "#cccccc",
      tertiaryBorderColor: "#3c3c3c",
      lineColor: "#9cdcfe",
      textColor: "#cccccc",
      mainBkg: "#252526",
      nodeBorder: "#3794ff",
      clusterBkg: "#181818",
      clusterBorder: "#3c3c3c",
      edgeLabelBackground: "#1e1e1e",
      fontFamily: "Inter, Segoe UI, system-ui, sans-serif",
    } : {
      background: "#ffffff",
      primaryColor: "#f3f3f3",
      primaryTextColor: "#333333",
      primaryBorderColor: "#007acc",
      secondaryColor: "#e8e8e8",
      secondaryTextColor: "#333333",
      secondaryBorderColor: "#0066b8",
      tertiaryColor: "#f8f8f8",
      tertiaryTextColor: "#333333",
      tertiaryBorderColor: "#d4d4d4",
      lineColor: "#0066b8",
      textColor: "#333333",
      mainBkg: "#f3f3f3",
      nodeBorder: "#007acc",
      clusterBkg: "#f8f8f8",
      clusterBorder: "#d4d4d4",
      edgeLabelBackground: "#ffffff",
      fontFamily: "Inter, Segoe UI, system-ui, sans-serif",
    },
  });
}

function unsafe_css(value: string): boolean {
  if (UNSAFE_CSS_PATTERN.test(value)) return true;
  for (const match of value.matchAll(/url\s*\(([^)]*)\)/giu)) {
    const reference = (match[1] ?? "").trim().replace(/^["']|["']$/gu, "");
    if (!LOCAL_REFERENCE_PATTERN.test(reference)) return true;
  }
  return false;
}

function sanitized_svg(svg_source: string): { svg: SVGSVGElement | null; detail: string } {
  if (new TextEncoder().encode(svg_source).byteLength > MAXIMUM_MERMAID_SVG_BYTES) {
    return { svg: null, detail: "svg_too_large" };
  }
  const parsed = new DOMParser().parseFromString(svg_source, "image/svg+xml");
  const source_root = parsed.documentElement;
  if (source_root.localName.toLowerCase() !== "svg" || parsed.querySelector("parsererror")) {
    return { svg: null, detail: "invalid_xml" };
  }
  let rejection = "unsafe_svg";
  let element_count = 0;

  const clone_node = (source: Element, depth: number): Element | null => {
    element_count += 1;
    if (depth > MAXIMUM_MERMAID_SVG_DEPTH || element_count > MAXIMUM_MERMAID_SVG_ELEMENTS) {
      rejection = "svg_structure_too_large";
      return null;
    }
    const local_name = source.localName.toLowerCase();
    if (!SAFE_SVG_ELEMENTS.has(local_name)) {
      rejection = `element:${local_name}`;
      return null;
    }
    const target = document.createElementNS(SVG_NAMESPACE, source.localName);
    for (const attribute of Array.from(source.attributes)) {
      const name = attribute.localName.toLowerCase();
      const value = attribute.value;
      if (value.length > MAXIMUM_MERMAID_SVG_ATTRIBUTE_LENGTH) {
        rejection = `attribute_too_large:${name}`;
        return null;
      }
      const safe_data_attribute = name.startsWith("data-") && value.length <= 512
        && !/[\u0000-\u001f\u007f]/u.test(value);
      if (name.startsWith("on") || (!SAFE_SVG_ATTRIBUTES.has(name) && !safe_data_attribute)) {
        rejection = `attribute:${name}`;
        return null;
      }
      if ((name === "href" || name === "marker-start" || name === "marker-mid" || name === "marker-end"
          || name === "clip-path" || name === "mask" || name === "filter")
          && value.includes("url(") && !/^url\(#[a-zA-Z0-9_.:-]{1,160}\)$/u.test(value)) {
        rejection = `reference:${name}`;
        return null;
      }
      if (name === "href" && !LOCAL_REFERENCE_PATTERN.test(value)) {
        rejection = "reference:href";
        return null;
      }
      if (name === "style" && unsafe_css(value)) {
        rejection = "css:attribute";
        return null;
      }
      target.setAttribute(attribute.name, value);
    }
    for (const child of Array.from(source.childNodes)) {
      if (child.nodeType === Node.TEXT_NODE) {
        const value = child.textContent ?? "";
        if (local_name === "style" && unsafe_css(value)) {
          rejection = "css:style";
          return null;
        }
        target.append(document.createTextNode(value));
      } else if (child.nodeType === Node.ELEMENT_NODE) {
        const cloned_child = clone_node(child as Element, depth + 1);
        if (!cloned_child) return null;
        target.append(cloned_child);
      }
    }
    return target;
  };

  const result = clone_node(source_root, 1);
  return result instanceof SVGSVGElement ? { svg: result, detail: "ok" } : { svg: null, detail: rejection };
}

function post(message: mermaid_frame_message): void {
  active_port?.postMessage(message);
}

function context_message(message_type: "mermaid_activate" | "mermaid_save_requested" | "mermaid_source_mode_requested"): void {
  const render = active_render;
  if (!render || !active_session_nonce) return;
  post({
    message_type,
    protocol_version: PREVIEW_PROTOCOL_VERSION,
    session_nonce: active_session_nonce,
    document_id: render.document_id,
    block_id: render.block.block_id,
    revision: render.revision,
  });
}

function apply_zoom(zoom: number, reset_scroll: boolean): void {
  current_zoom = Math.max(MINIMUM_ZOOM, Math.min(MAXIMUM_ZOOM, zoom));
  if (active_svg) {
    active_svg.style.width = `${Math.round(current_zoom * 100)}%`;
    active_svg.style.maxWidth = "none";
    active_svg.style.height = "auto";
  }
  const zoom_value = document.getElementById("mermaid_zoom_value");
  if (zoom_value instanceof HTMLButtonElement) {
    const label = `${Math.round(current_zoom * 100)}%`;
    zoom_value.textContent = label;
    zoom_value.setAttribute("aria-label", `当前缩放 ${label}；复位 Mermaid 图表`);
  }
  const zoom_out = document.querySelector<HTMLButtonElement>('[data-mermaid-action="zoom_out"]');
  const zoom_in = document.querySelector<HTMLButtonElement>('[data-mermaid-action="zoom_in"]');
  if (zoom_out) zoom_out.disabled = current_zoom <= MINIMUM_ZOOM;
  if (zoom_in) zoom_in.disabled = current_zoom >= MAXIMUM_ZOOM;
  if (reset_scroll) {
    document.getElementById("mermaid_viewport")?.scrollTo({ left: 0, top: 0 });
  }
}

async function report_rendered_height(): Promise<void> {
  const message = active_render;
  const root = document.getElementById("mermaid_root");
  if (!message || !root) return;
  await new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));
  const viewport = document.getElementById("mermaid_viewport");
  if (viewport instanceof HTMLElement && !viewport.style.height) {
    viewport.style.height = `${Math.max(96, viewport.clientHeight)}px`;
  }
  const height = Math.max(130, Math.min(10_000, Math.ceil(root.scrollHeight)));
  post({
    message_type: "mermaid_rendered",
    protocol_version: PREVIEW_PROTOCOL_VERSION,
    session_nonce: message.session_nonce,
    document_id: message.document_id,
    block_id: message.block.block_id,
    revision: message.revision,
    height,
  });
}

function set_zoom(zoom: number, reset_scroll = false): void {
  apply_zoom(zoom, reset_scroll);
}

function render_error(
  message: mermaid_render_message,
  error_code: "MERMAID_DOCUMENT_CONFIG_REJECTED" | "MERMAID_RENDER_FAILED" | "UNSAFE_MERMAID_SVG",
  detail: string,
): void {
  const canvas = document.getElementById("mermaid_canvas");
  active_svg = null;
  if (canvas) {
    canvas.replaceChildren();
    const error = document.createElement("p");
    error.className = "mermaid_error";
    error.textContent = "Mermaid 图表无法安全渲染；点击返回源码";
    canvas.append(error);
  }
  post({
    message_type: "mermaid_render_error",
    protocol_version: PREVIEW_PROTOCOL_VERSION,
    session_nonce: message.session_nonce,
    document_id: message.document_id,
    block_id: message.block.block_id,
    revision: message.revision,
    error_code,
    detail,
  });
}

async function render_mermaid(message: mermaid_render_message): Promise<void> {
  const root = document.getElementById("mermaid_root");
  const canvas = document.getElementById("mermaid_canvas");
  if (!root || !canvas) return;
  active_render = message;
  if (mermaid_source_has_document_configuration(message.block.mermaid_source)) {
    render_error(message, "MERMAID_DOCUMENT_CONFIG_REJECTED", "document_configuration");
    return;
  }
  try {
    document.documentElement.dataset.theme = message.theme;
    initialize_mermaid(message.theme);
    const render_id = `loop_mermaid_${message.block.content_hash}_${message.revision}_${message.theme}`;
    const rendered = await mermaid.render(render_id, message.block.mermaid_source);
    const sanitized = sanitized_svg(rendered.svg);
    if (!sanitized.svg) {
      render_error(message, "UNSAFE_MERMAID_SVG", sanitized.detail);
      return;
    }
    const svg = sanitized.svg;
    svg.removeAttribute("height");
    svg.removeAttribute("width");
    svg.setAttribute("role", "img");
    svg.setAttribute("aria-label", "Mermaid 图表；点击编辑源码");
    active_svg = svg;
    canvas.replaceChildren(svg);
    apply_zoom(1, true);
    await report_rendered_height();
  } catch {
    render_error(message, "MERMAID_RENDER_FAILED", "renderer_exception");
  }
}

function connect(event: MessageEvent<unknown>): void {
  if (active_port || event.source !== window.parent || !is_mermaid_connect_message(event.data) || event.ports.length !== 1) return;
  active_session_nonce = event.data.session_nonce;
  active_port = event.ports[0] ?? null;
  window.removeEventListener("message", connect);
  if (!active_port) return;
  active_port.onmessage = (message_event: MessageEvent<unknown>) => {
    if (!active_session_nonce || !is_mermaid_render_message(message_event.data, active_session_nonce)) {
      active_port?.close();
      active_port = null;
      return;
    }
    void render_mermaid(message_event.data);
  };
  active_port.start();
  post({
    message_type: "mermaid_ready",
    protocol_version: PREVIEW_PROTOCOL_VERSION,
    session_nonce: active_session_nonce,
  });
}

document.documentElement.dataset.previewRuntime = "ready";
apply_zoom(1, false);
window.addEventListener("message", connect);
window.addEventListener("keydown", (event) => {
  if ((event.ctrlKey || event.metaKey) && !event.altKey && event.key.toLowerCase() === "s") {
    event.preventDefault();
    context_message("mermaid_save_requested");
  } else if ((event.ctrlKey || event.metaKey) && !event.altKey && !event.shiftKey
      && (event.key === "/" || event.code === "Slash")) {
    event.preventDefault();
    context_message("mermaid_source_mode_requested");
  } else if ((event.ctrlKey || event.metaKey) && !event.altKey
      && (event.key === "+" || event.key === "=" || event.code === "NumpadAdd")) {
    event.preventDefault();
    set_zoom(current_zoom + ZOOM_STEP);
  } else if ((event.ctrlKey || event.metaKey) && !event.altKey
      && (event.key === "-" || event.code === "NumpadSubtract")) {
    event.preventDefault();
    set_zoom(current_zoom - ZOOM_STEP);
  } else if ((event.ctrlKey || event.metaKey) && !event.altKey && event.key === "0") {
    event.preventDefault();
    set_zoom(1, true);
  } else if ((event.key === "Enter" || event.key === " ")
      && event.target === document.getElementById("mermaid_viewport")) {
    event.preventDefault();
    context_message("mermaid_activate");
  }
});
document.getElementById("mermaid_canvas")?.addEventListener("pointerdown", (event) => {
  if (event.button !== 0) return;
  event.preventDefault();
  context_message("mermaid_activate");
});
for (const button of document.querySelectorAll<HTMLButtonElement>("button[data-mermaid-action]")) {
  button.addEventListener("click", () => {
    if (button.disabled) return;
    const action = button.dataset.mermaidAction;
    if (action === "zoom_out") set_zoom(current_zoom - ZOOM_STEP);
    else if (action === "zoom_in") set_zoom(current_zoom + ZOOM_STEP);
    else if (action === "reset" || action === "fit") set_zoom(1, true);
    else if (action === "source") context_message("mermaid_activate");
  });
}
document.documentElement.dataset.previewControls = "ready";
