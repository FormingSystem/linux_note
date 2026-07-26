import { useEffect, useLayoutEffect, useRef, useState } from "react";
import MermaidViewer from "./MermaidViewer";

let diagramSequence = 0;
let viewerSequence = 0;
let mermaidReady: Promise<typeof import("mermaid").default> | undefined;

function loadMermaid() {
  if (!mermaidReady) {
    mermaidReady = import("mermaid").then(({ default: mermaid }) => mermaid);
  }
  return mermaidReady;
}

export default function MermaidDiagram({ source }: { source: string }) {
  const id = useRef(`loop-mermaid-${++diagramSequence}`);
  const canvasRef = useRef<HTMLDivElement>(null);
  const [svg, setSvg] = useState("");
  const [viewerSvg, setViewerSvg] = useState("");
  const [error, setError] = useState("");
  const [theme, setTheme] = useState<"light" | "dark">(() => document.documentElement.dataset.theme === "dark" ? "dark" : "light");

  useEffect(() => {
    const updateTheme = (event: Event) => setTheme((event as CustomEvent<"light" | "dark">).detail);
    window.addEventListener("loop-theme-change", updateTheme);
    return () => window.removeEventListener("loop-theme-change", updateTheme);
  }, []);

  useEffect(() => {
    let active = true;
    setSvg("");
    setError("");
    void loadMermaid()
      .then((mermaid) => {
        mermaid.initialize(mermaidConfig(theme));
        return mermaid.render(id.current, source);
      })
      .then((result) => {
        if (active) setSvg(normalizeSvgSize(result.svg));
      })
      .catch((reason: unknown) => {
        if (active) setError(reason instanceof Error ? reason.message : "Mermaid 图表语法无效");
      });
    return () => { active = false; };
  }, [source, theme]);

  useLayoutEffect(() => {
    if (!svg) return;
    const frame = requestAnimationFrame(() => {
      const rendered = canvasRef.current?.querySelector("svg");
      if (rendered) fitSvgViewBoxToContent(rendered);
    });
    return () => cancelAnimationFrame(frame);
  }, [svg, viewerSvg]);

  if (error) {
    return <figure className="mermaid-diagram mermaid-error"><figcaption>Mermaid 图表无法渲染：{error}</figcaption><pre><code>{source}</code></pre></figure>;
  }
  if (!svg) return <div className="mermaid-diagram mermaid-loading">正在渲染图表……</div>;
  const openViewer = () => {
    const renderedSvg = canvasRef.current?.querySelector("svg")?.outerHTML;
    if (!renderedSvg) return;
    setViewerSvg(namespaceSvgIds(renderedSvg, `loop-mermaid-viewer-${++viewerSequence}`));
  };

  return <>
  <figure className="mermaid-diagram">
    <button
      type="button"
      className="mermaid-fullscreen-button"
      title="全屏查看 Mermaid 图表"
      onClick={openViewer}
    >
      <span aria-hidden="true">⛶</span>
      全屏查看
    </button>
    <div ref={canvasRef} className="mermaid-canvas">
      <div className="mermaid-content" dangerouslySetInnerHTML={{ __html: svg }} />
    </div>
  </figure>
  {viewerSvg && <MermaidViewer svg={viewerSvg} onClose={() => setViewerSvg("")} />}
  </>;
}

function mermaidConfig(theme: "light" | "dark") {
  const dark = theme === "dark";
  return {
    startOnLoad: false,
    securityLevel: "strict" as const,
    theme: "base" as const,
    themeVariables: {
      background: dark ? "#212121" : "#ffffff",
      primaryColor: dark ? "#343541" : "#f7f7f8",
      primaryTextColor: dark ? "#ececf1" : "#2f2f2f",
      primaryBorderColor: dark ? "#565869" : "#cfd5d2",
      lineColor: dark ? "#acacbe" : "#6b7280",
      secondaryColor: dark ? "#2f2f2f" : "#ffffff",
      tertiaryColor: dark ? "#173f35" : "#eef7f4",
      clusterBkg: dark ? "#2f2f2f" : "#f7f7f8",
      clusterBorder: dark ? "#565869" : "#d8ddda",
      edgeLabelBackground: dark ? "#212121" : "#ffffff",
      fontFamily: '"Noto Sans SC", system-ui, sans-serif',
    },
    flowchart: { htmlLabels: false },
  };
}

function normalizeSvgSize(source: string) {
  const template = document.createElement("template");
  template.innerHTML = source.trim();
  const svg = template.content.querySelector("svg");
  if (!svg) return source;
  const viewBox = svg.getAttribute("viewBox")?.trim().split(/\s+/).map(Number);
  if (viewBox?.length === 4 && viewBox.every(Number.isFinite)) {
    const width = Math.max(1, Math.ceil(viewBox[2]));
    const height = Math.max(1, Math.ceil(viewBox[3]));
    svg.setAttribute("width", String(width));
    svg.setAttribute("height", String(height));
    svg.setAttribute("preserveAspectRatio", "xMidYMid meet");
  }
  svg.style.removeProperty("width");
  svg.style.removeProperty("height");
  svg.style.removeProperty("max-width");
  svg.style.backgroundColor = "transparent";
  return svg.outerHTML;
}

function namespaceSvgIds(source: string, prefix: string) {
  const template = document.createElement("template");
  template.innerHTML = source.trim();
  const svg = template.content.querySelector("svg");
  if (!svg) return source;
  const replacements = new Map<string, string>();

  svg.querySelectorAll<SVGElement>("[id]").forEach((element) => {
    const oldId = element.id;
    const newId = `${prefix}-${oldId}`;
    replacements.set(oldId, newId);
    element.id = newId;
  });

  svg.querySelectorAll<SVGElement>("*").forEach((element) => {
    for (const attribute of Array.from(element.attributes)) {
      let value = attribute.value;
      replacements.forEach((newId, oldId) => {
        value = value
          .replaceAll(`url(#${oldId})`, `url(#${newId})`)
          .replaceAll(`#${oldId}`, `#${newId}`);
      });
      if (value !== attribute.value) element.setAttribute(attribute.name, value);
    }
  });

  svg.querySelectorAll("style").forEach((style) => {
    let value = style.textContent ?? "";
    replacements.forEach((newId, oldId) => {
      value = value.replaceAll(`#${oldId}`, `#${newId}`);
    });
    style.textContent = value;
  });

  return svg.outerHTML;
}

function fitSvgViewBoxToContent(svg: SVGSVGElement) {
  try {
    const bounds = svg.getBBox();
    if (!bounds.width || !bounds.height) return;
    const padding = Math.max(12, Math.min(32, Math.max(bounds.width, bounds.height) * 0.025));
    const width = Math.ceil(bounds.width + padding * 2);
    const height = Math.ceil(bounds.height + padding * 2);
    svg.setAttribute("viewBox", [
      bounds.x - padding,
      bounds.y - padding,
      width,
      height,
    ].join(" "));
    svg.setAttribute("width", String(width));
    svg.setAttribute("height", String(height));
    svg.setAttribute("preserveAspectRatio", "xMidYMid meet");
  } catch {
    // 浏览器无法测量尚未布局的 SVG 时保留 Mermaid 原始边界。
  }
}
