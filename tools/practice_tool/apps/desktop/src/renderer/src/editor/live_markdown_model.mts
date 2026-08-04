import type { preview_source_span } from "@loop/markdown-engine/contracts";

export function source_span_is_active(
  span: preview_source_span,
  ranges: readonly { from: number; to: number }[],
): boolean {
  return ranges.some((range) => range.from <= span.end && range.to >= span.start);
}
