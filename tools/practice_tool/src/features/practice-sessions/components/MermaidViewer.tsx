import { useEffect, useLayoutEffect, useRef, useState, type CSSProperties, type PointerEvent as ReactPointerEvent } from "react";
import { createPortal } from "react-dom";

const MIN_ZOOM = 0.2;
const MAX_ZOOM = 6;

type View = { scale: number; x: number; y: number };

export default function MermaidViewer({ svg, onClose }: { svg: string; onClose: () => void }) {
  const canvasRef = useRef<HTMLDivElement>(null);
  const drag = useRef({ pointerId: 0, startX: 0, startY: 0, originX: 0, originY: 0 });
  const [dragging, setDragging] = useState(false);
  const [view, setView] = useState<View>({ scale: 1, x: 0, y: 0 });

  useEffect(() => {
    const closeOnEscape = (event: KeyboardEvent) => {
      if (event.key !== "Escape") return;
      event.preventDefault();
      onClose();
    };
    window.addEventListener("keydown", closeOnEscape);
    return () => window.removeEventListener("keydown", closeOnEscape);
  }, [onClose]);

  useLayoutEffect(() => {
    const canvas = canvasRef.current;
    const fit = () => {
      const rendered = canvas?.querySelector("svg");
      if (rendered) fitSvgViewBoxToContent(rendered);
      fitDiagramToViewport(canvas, setView);
    };
    const frame = requestAnimationFrame(fit);
    const observer = new ResizeObserver(fit);
    if (canvas) observer.observe(canvas);
    return () => {
      cancelAnimationFrame(frame);
      observer.disconnect();
    };
  }, [svg]);

  const changeZoom = (factor: number) => setView((current) => ({
    ...current,
    scale: clamp(current.scale * factor, MIN_ZOOM, MAX_ZOOM),
  }));
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const handleWheel = (event: WheelEvent) => {
      event.preventDefault();
      event.stopPropagation();
      if (!event.ctrlKey) return;
      const bounds = canvas.getBoundingClientRect();
      const pointerX = event.clientX - bounds.left - bounds.width / 2;
      const pointerY = event.clientY - bounds.top - bounds.height / 2;
      setView((current) => {
        const nextScale = clamp(current.scale * Math.exp(-event.deltaY * 0.002), MIN_ZOOM, MAX_ZOOM);
        const ratio = nextScale / current.scale;
        return {
          scale: nextScale,
          x: pointerX - (pointerX - current.x) * ratio,
          y: pointerY - (pointerY - current.y) * ratio,
        };
      });
    };
    canvas.addEventListener("wheel", handleWheel, { passive: false });
    return () => canvas.removeEventListener("wheel", handleWheel);
  }, []);
  const handlePointerDown = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (event.button !== 0) return;
    drag.current = {
      pointerId: event.pointerId,
      startX: event.clientX,
      startY: event.clientY,
      originX: view.x,
      originY: view.y,
    };
    event.currentTarget.setPointerCapture(event.pointerId);
    setDragging(true);
  };
  const handlePointerMove = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!dragging || event.pointerId !== drag.current.pointerId) return;
    setView((current) => ({
      ...current,
      x: drag.current.originX + event.clientX - drag.current.startX,
      y: drag.current.originY + event.clientY - drag.current.startY,
    }));
  };
  const handlePointerUp = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (event.pointerId !== drag.current.pointerId) return;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
    setDragging(false);
  };
  const style = {
    "--mermaid-scale": String(view.scale),
    "--mermaid-pan-x": `${view.x}px`,
    "--mermaid-pan-y": `${view.y}px`,
  } as CSSProperties;

  return createPortal(
    <div className={`mermaid-viewer ${dragging ? "is-dragging" : ""}`} role="dialog" aria-modal="true" aria-label="Mermaid 图表全屏查看">
      <div className="mermaid-view-controls" aria-label="图表缩放控制">
        <button type="button" title="缩小" onClick={() => changeZoom(0.8)}>−</button>
        <span>{Math.round(view.scale * 100)}%</span>
        <button type="button" title="放大" onClick={() => changeZoom(1.25)}>＋</button>
        <button type="button" onClick={() => fitDiagramToViewport(canvasRef.current, setView)}>适应屏幕</button>
      </div>
      <button type="button" className="mermaid-viewer-close" onClick={onClose}><span aria-hidden="true">×</span> 退出全屏</button>
      <div
        ref={canvasRef}
        className="mermaid-viewer-canvas"
        style={style}
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={handlePointerUp}
        onPointerCancel={handlePointerUp}
      >
        <div className="mermaid-viewer-positioner">
          <div className="mermaid-viewer-content" dangerouslySetInnerHTML={{ __html: svg }} />
        </div>
      </div>
      <div className="mermaid-viewer-hint">Ctrl + 滚轮缩放 · 按住左键拖动 · Esc 退出</div>
    </div>,
    document.body,
  );
}

function fitDiagramToViewport(canvas: HTMLDivElement | null, update: (value: View) => void) {
  const svg = canvas?.querySelector("svg");
  if (!canvas || !svg) return;
  const width = svg.viewBox.baseVal.width || Number(svg.getAttribute("width"));
  const height = svg.viewBox.baseVal.height || Number(svg.getAttribute("height"));
  if (!width || !height) return;
  const bounds = canvas.getBoundingClientRect();
  update({
    scale: clamp(Math.min(bounds.width / width, bounds.height / height) * 0.88, MIN_ZOOM, MAX_ZOOM),
    x: 0,
    y: 0,
  });
}

function clamp(value: number, minimum: number, maximum: number) {
  return Math.min(maximum, Math.max(minimum, value));
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
