import { RangeSetBuilder, StateEffect, StateField, type ChangeDesc, type Extension } from "@codemirror/state";
import { Decoration, EditorView, WidgetType, type DecorationSet } from "@codemirror/view";
import {
  is_preview_document,
  type preview_block,
  type preview_document,
  type preview_markdown_block,
  type preview_mermaid_block,
  type preview_source_span,
} from "@loop/markdown-engine/contracts";
import {
  create_mermaid_connect_message,
  create_mermaid_render_message,
  is_mermaid_frame_message,
} from "../preview/preview_frame_protocol.mts";
import { render_safe_hast } from "../preview/safe_hast_dom";
import type { workbench_theme } from "../theme/workbench_theme.mts";
import { source_span_is_active } from "./live_markdown_model.mts";

interface live_markdown_callbacks {
  on_save_requested(): void;
  on_source_mode_requested(): void;
  on_render_error(message: string): void;
}

interface live_markdown_state {
  blocks: readonly preview_block[];
  active: boolean;
  source_mode: boolean;
  theme: workbench_theme;
  decorations: DecorationSet;
}

const set_preview_document_effect = StateEffect.define<preview_document | null>();
const set_active_effect = StateEffect.define<boolean>();
const set_source_mode_effect = StateEffect.define<boolean>();
const set_theme_effect = StateEffect.define<workbench_theme>();
const mermaid_disposers = new WeakMap<HTMLElement, () => void>();

function create_session_nonce(): string {
  const bytes = new Uint8Array(16);
  crypto.getRandomValues(bytes);
  return Array.from(bytes, (value) => value.toString(16).padStart(2, "0")).join("");
}

function activate_block(view: EditorView, span: preview_source_span): void {
  const anchor = Math.max(0, Math.min(span.start, view.state.doc.length));
  view.dispatch({ selection: { anchor }, scrollIntoView: true });
  view.focus();
}

function map_block(block: preview_block, changes: ChangeDesc): preview_block | null {
  if (changes.touchesRange(block.source_span.start, block.source_span.end)) return null;
  const start = changes.mapPos(block.source_span.start, -1);
  const end = changes.mapPos(block.source_span.end, 1);
  if (end <= start) return null;
  return { ...block, source_span: { start, end } };
}

class safe_markdown_widget extends WidgetType {
  readonly #block: preview_markdown_block;

  constructor(block: preview_markdown_block) {
    super();
    this.#block = block;
  }

  override eq(other: safe_markdown_widget): boolean {
    return other.#block.block_id === this.#block.block_id
      && other.#block.content_hash === this.#block.content_hash
      && other.#block.source_span.start === this.#block.source_span.start
      && other.#block.source_span.end === this.#block.source_span.end;
  }

  override toDOM(view: EditorView): HTMLElement {
    const target = document.createElement("div");
    target.className = "loop_live_markdown_block";
    target.tabIndex = 0;
    target.setAttribute("role", "button");
    target.setAttribute("aria-label", "已渲染 Markdown 块，按 Enter 编辑源码");
    target.dataset.blockId = this.#block.block_id;
    render_safe_hast(target, this.#block.safe_hast);
    const activate = (event: Event): void => {
      event.preventDefault();
      event.stopPropagation();
      activate_block(view, this.#block.source_span);
    };
    target.addEventListener("pointerdown", (event) => {
      if (event.button === 0) activate(event);
    });
    target.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") activate(event);
    });
    return target;
  }

  override ignoreEvent(): boolean {
    return true;
  }

  override get estimatedHeight(): number {
    return 42;
  }
}

class mermaid_widget extends WidgetType {
  readonly #document_id: string;
  readonly #revision: number;
  readonly #block: preview_mermaid_block;
  readonly #theme: workbench_theme;
  readonly #callbacks: live_markdown_callbacks;

  constructor(
    document_id: string,
    revision: number,
    block: preview_mermaid_block,
    theme: workbench_theme,
    callbacks: live_markdown_callbacks,
  ) {
    super();
    this.#document_id = document_id;
    this.#revision = revision;
    this.#block = block;
    this.#theme = theme;
    this.#callbacks = callbacks;
  }

  override eq(other: mermaid_widget): boolean {
    return other.#document_id === this.#document_id
      && other.#revision === this.#revision
      && other.#block.block_id === this.#block.block_id
      && other.#block.content_hash === this.#block.content_hash
      && other.#theme === this.#theme
      && other.#block.source_span.start === this.#block.source_span.start
      && other.#block.source_span.end === this.#block.source_span.end;
  }

  override toDOM(view: EditorView): HTMLElement {
    const wrapper = document.createElement("div");
    wrapper.className = "loop_mermaid_block";
    wrapper.dataset.blockId = this.#block.block_id;
    const frame = document.createElement("iframe");
    frame.className = "loop_mermaid_frame";
    frame.title = "Mermaid 图表；点击编辑源码";
    frame.src = "loop-preview://preview/";
    frame.setAttribute("sandbox", "allow-scripts");
    wrapper.append(frame);

    const session_nonce = create_session_nonce();
    let port: MessagePort | null = null;
    let layout_timer: ReturnType<typeof setTimeout> | null = null;
    let disposed = false;
    const show_error = (message: string): void => {
      wrapper.classList.remove("is_loading");
      wrapper.classList.add("has_error");
      let error = wrapper.querySelector<HTMLElement>(".loop_mermaid_error");
      if (!error) {
        error = document.createElement("div");
        error.className = "loop_mermaid_error";
        error.tabIndex = 0;
        wrapper.append(error);
      }
      error.textContent = `${message}；点击查看 Mermaid 源码`;
      error.addEventListener("pointerdown", (event) => {
        event.preventDefault();
        activate_block(view, this.#block.source_span);
      }, { once: true });
      this.#callbacks.on_render_error(message);
    };
    const on_load = (): void => {
      const connect_when_laid_out = (attempt: number): void => {
        if (disposed || !frame.contentWindow) return;
        if (!wrapper.isConnected || frame.getBoundingClientRect().width < 16) {
          if (attempt >= 120) {
            show_error("Mermaid 图表等待布局超时");
            return;
          }
          layout_timer = setTimeout(() => connect_when_laid_out(attempt + 1), 16);
          return;
        }
        const channel = new MessageChannel();
        port = channel.port1;
        port.onmessage = (event: MessageEvent<unknown>) => {
        if (!is_mermaid_frame_message(event.data, session_nonce)) {
          show_error("Mermaid Frame 返回了无效消息");
          port?.close();
          port = null;
          return;
        }
        const message = event.data;
        if (message.message_type === "mermaid_ready") {
          port?.postMessage(create_mermaid_render_message(
            session_nonce,
            this.#document_id,
            this.#revision,
            this.#block,
            this.#theme,
          ));
          return;
        }
        if (message.document_id !== this.#document_id || message.block_id !== this.#block.block_id
            || message.revision !== this.#revision) {
          show_error("Mermaid Frame 响应与当前块不匹配");
          return;
        }
        if (message.message_type === "mermaid_rendered") {
          frame.style.height = `${message.height}px`;
          wrapper.classList.remove("is_loading");
        } else if (message.message_type === "mermaid_render_error") {
          wrapper.dataset.mermaidError = message.error_code;
          wrapper.dataset.mermaidErrorDetail = message.detail;
          show_error("Mermaid 图表渲染失败");
        } else if (message.message_type === "mermaid_activate") {
          activate_block(view, this.#block.source_span);
        } else if (message.message_type === "mermaid_save_requested") {
          this.#callbacks.on_save_requested();
        } else if (message.message_type === "mermaid_source_mode_requested") {
          this.#callbacks.on_source_mode_requested();
        }
        };
        port.start();
        frame.contentWindow.postMessage(create_mermaid_connect_message(session_nonce), "*", [channel.port2]);
      };
      connect_when_laid_out(0);
    };
    wrapper.classList.add("is_loading");
    frame.addEventListener("load", on_load, { once: true });
    mermaid_disposers.set(wrapper, () => {
      disposed = true;
      if (layout_timer) clearTimeout(layout_timer);
      layout_timer = null;
      port?.close();
      port = null;
      frame.removeEventListener("load", on_load);
    });
    return wrapper;
  }

  override destroy(dom: HTMLElement): void {
    mermaid_disposers.get(dom)?.();
    mermaid_disposers.delete(dom);
  }

  override ignoreEvent(): boolean {
    return true;
  }

  override get estimatedHeight(): number {
    return 180;
  }
}

function build_decorations(
  state: live_markdown_state,
  view_state: EditorView["state"],
  document_id: string,
  revision: number,
  callbacks: live_markdown_callbacks,
): DecorationSet {
  if (!state.active || state.source_mode) return Decoration.none;
  const builder = new RangeSetBuilder<Decoration>();
  const selection_ranges = view_state.selection.ranges.map((range) => ({ from: range.from, to: range.to }));
  for (const block of state.blocks) {
    const { start, end } = block.source_span;
    if (start < 0 || end > view_state.doc.length || end <= start
        || source_span_is_active(block.source_span, selection_ranges)) continue;
    const widget = block.kind === "markdown"
      ? new safe_markdown_widget(block)
      : new mermaid_widget(document_id, revision, block, state.theme, callbacks);
    builder.add(start, end, Decoration.replace({ widget, block: true, inclusive: false }));
  }
  return builder.finish();
}

export function live_markdown_surface(
  document_id: string,
  initial_theme: workbench_theme,
  initial_active: boolean,
  callbacks: live_markdown_callbacks,
): Extension {
  let current_revision = 0;
  const field = StateField.define<live_markdown_state>({
    create: () => ({
      blocks: [],
      active: initial_active,
      source_mode: false,
      theme: initial_theme,
      decorations: Decoration.none,
    }),
    update: (value, transaction) => {
      let blocks = value.blocks;
      let active = value.active;
      let source_mode = value.source_mode;
      let theme = value.theme;
      if (transaction.docChanged) {
        blocks = blocks.flatMap((block) => {
          const mapped = map_block(block, transaction.changes);
          return mapped ? [mapped] : [];
        });
      }
      for (const effect of transaction.effects) {
        if (effect.is(set_preview_document_effect)) {
          const document = effect.value;
          if (document && document.document_id === document_id
              && document.source_length === transaction.state.doc.length) {
            blocks = document.blocks;
            current_revision = document.revision;
          }
        } else if (effect.is(set_active_effect)) {
          active = effect.value;
        } else if (effect.is(set_source_mode_effect)) {
          source_mode = effect.value;
        } else if (effect.is(set_theme_effect)) {
          theme = effect.value;
        }
      }
      const next = { blocks, active, source_mode, theme, decorations: Decoration.none };
      return {
        ...next,
        decorations: build_decorations(next, transaction.state, document_id, current_revision, callbacks),
      };
    },
    provide: (value) => EditorView.decorations.from(value, (state) => state.decorations),
  });
  return field;
}

export function set_live_preview_document(view: EditorView, document: preview_document | null): void {
  if (document && !is_preview_document(document)) throw new Error("拒绝无效 Markdown 块文档");
  view.dispatch({ effects: set_preview_document_effect.of(document) });
}

export function set_live_active(view: EditorView, active: boolean): void {
  view.dispatch({ effects: set_active_effect.of(active) });
}

export function set_live_source_mode(view: EditorView, source_mode: boolean): void {
  view.dispatch({ effects: set_source_mode_effect.of(source_mode) });
}

export function set_live_theme(view: EditorView, theme: workbench_theme): void {
  view.dispatch({ effects: set_theme_effect.of(theme) });
}
