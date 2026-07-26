import { useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";

type TooltipState = {
  text: string;
  left: number;
  top: number;
  above: boolean;
};

const TOOLTIP_DELAY_MS = 450;
const TOOLTIP_HALF_WIDTH = 170;

export default function TooltipLayer() {
  const [tooltip, setTooltip] = useState<TooltipState>();
  const timerRef = useRef<number | undefined>(undefined);
  const anchorRef = useRef<HTMLElement | undefined>(undefined);

  useEffect(() => {
    const hide = () => {
      window.clearTimeout(timerRef.current);
      anchorRef.current = undefined;
      setTooltip(undefined);
    };
    const schedule = (target: EventTarget | null) => {
      const element = target instanceof Element ? target.closest<HTMLElement>("[data-tooltip]") : null;
      const text = element?.dataset.tooltip?.trim();
      if (!element || !text || element === anchorRef.current) return;
      hide();
      anchorRef.current = element;
      timerRef.current = window.setTimeout(() => {
        const rect = element.getBoundingClientRect();
        const above = rect.bottom + 96 > window.innerHeight && rect.top > 96;
        setTooltip({
          text,
          left: Math.min(window.innerWidth - TOOLTIP_HALF_WIDTH, Math.max(TOOLTIP_HALF_WIDTH, rect.left + rect.width / 2)),
          top: above ? rect.top - 10 : rect.bottom + 10,
          above,
        });
      }, TOOLTIP_DELAY_MS);
    };
    const leave = (event: PointerEvent) => {
      const anchor = anchorRef.current;
      if (!anchor || (event.relatedTarget instanceof Node && anchor.contains(event.relatedTarget))) return;
      hide();
    };
    const focusOut = (event: FocusEvent) => {
      const anchor = anchorRef.current;
      if (!anchor || (event.relatedTarget instanceof Node && anchor.contains(event.relatedTarget))) return;
      hide();
    };
    const pointerOver = (event: PointerEvent) => schedule(event.target);
    const focusIn = (event: FocusEvent) => schedule(event.target);

    document.addEventListener("pointerover", pointerOver);
    document.addEventListener("pointerout", leave);
    document.addEventListener("focusin", focusIn);
    document.addEventListener("focusout", focusOut);
    document.addEventListener("scroll", hide, true);
    window.addEventListener("resize", hide);
    return () => {
      hide();
      document.removeEventListener("pointerover", pointerOver);
      document.removeEventListener("pointerout", leave);
      document.removeEventListener("focusin", focusIn);
      document.removeEventListener("focusout", focusOut);
      document.removeEventListener("scroll", hide, true);
      window.removeEventListener("resize", hide);
    };
  }, []);

  if (!tooltip) return null;
  return createPortal(
    <div
      className={`global-tooltip ${tooltip.above ? "above" : "below"}`}
      role="tooltip"
      style={{ left: tooltip.left, top: tooltip.top }}
    >
      {tooltip.text}
    </div>,
    document.body,
  );
}
