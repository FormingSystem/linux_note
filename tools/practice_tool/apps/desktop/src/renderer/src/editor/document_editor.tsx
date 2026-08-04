import { useEffect, useRef } from "react";
import { redo, undo } from "@codemirror/commands";
import { markdown } from "@codemirror/lang-markdown";
import { Compartment, EditorState, type Text } from "@codemirror/state";
import { basicSetup, EditorView } from "codemirror";
import type { document_snapshot } from "@loop/ipc-contracts";
import type { preview_document } from "@loop/markdown-engine/contracts";
import {
  live_markdown_surface,
  set_live_active,
  set_live_preview_document,
  set_live_source_mode,
  set_live_theme,
} from "./live_markdown_surface";
import type { workbench_theme } from "../theme/workbench_theme.mts";

function create_editor_theme(theme: workbench_theme) {
  const dark = theme === "dark";
  return EditorView.theme({
    "&": { height: "100%", backgroundColor: dark ? "#1e1e1e" : "#ffffff", color: dark ? "#cccccc" : "#333333" },
    ".cm-scroller": { overflow: "auto", fontFamily: '"Cascadia Code", "SFMono-Regular", Consolas, monospace' },
    ".cm-content": { padding: "20px 0 40px", caretColor: dark ? "#aeafad" : "#000000" },
    ".cm-line": { padding: "0 28px" },
    ".cm-gutters": {
      backgroundColor: dark ? "#1e1e1e" : "#ffffff",
      color: dark ? "#858585" : "#237893",
      border: "none",
    },
    ".cm-activeLine, .cm-activeLineGutter": { backgroundColor: dark ? "#2a2d2e" : "#f0f0f0" },
    "&.cm-focused": { outline: "none" },
    ".cm-selectionBackground, &.cm-focused .cm-selectionBackground": {
      backgroundColor: `${dark ? "#264f78" : "#add6ff"} !important`,
    },
  }, { dark });
}

export interface editor_position {
  line: number;
  column: number;
}

interface document_editor_properties {
  snapshot: document_snapshot;
  active: boolean;
  editable: boolean;
  preview_document: preview_document | null;
  source_mode: boolean;
  theme: workbench_theme;
  on_document_change(document_id: string, content_equals_baseline: boolean): number;
  on_preview_source(document_id: string, revision: number, source: string): void;
  on_position_change(document_id: string, position: editor_position): void;
  on_save_requested(document_id: string): void;
  on_source_mode_requested(): void;
  on_preview_error(message: string): void;
  on_editor_handle_change(document_id: string, handle: document_editor_handle | null): void;
}

export interface document_editor_handle {
  undo(): boolean;
  redo(): boolean;
  set_editable(editable: boolean): void;
  prepare_save(): { content: string; editor_revision: number };
  accept_saved(editor_revision: number): boolean;
  reject_saved(editor_revision: number): void;
}

export function DocumentEditor(properties: document_editor_properties) {
  const host_ref = useRef<HTMLDivElement>(null);
  const view_ref = useRef<EditorView | null>(null);
  const request_preview_ref = useRef<(() => void) | null>(null);
  const editor_revision_ref = useRef(0);
  const editable_compartment_ref = useRef(new Compartment());
  const read_only_compartment_ref = useRef(new Compartment());
  const theme_compartment_ref = useRef(new Compartment());
  const callbacks_ref = useRef(properties);
  callbacks_ref.current = properties;

  useEffect(() => {
    const host = host_ref.current;
    if (!host) return undefined;
    const initial_state = EditorState.create({ doc: properties.snapshot.content });
    let baseline_document = initial_state.doc;
    const save_documents = new Map<number, Text>();
    let preview_timeout: ReturnType<typeof setTimeout> | null = null;
    const schedule_preview = (revision: number, view: EditorView): void => {
      if (preview_timeout) clearTimeout(preview_timeout);
      preview_timeout = setTimeout(() => {
        preview_timeout = null;
        callbacks_ref.current.on_preview_source(
          properties.snapshot.document_id,
          revision,
          view.state.doc.toString(),
        );
      }, revision === 0 ? 0 : 150);
    };
    const view = new EditorView({
      parent: host,
      state: EditorState.create({
        doc: baseline_document,
        extensions: [
          basicSetup,
          markdown(),
          editable_compartment_ref.current.of(EditorView.editable.of(properties.editable)),
          read_only_compartment_ref.current.of(EditorState.readOnly.of(!properties.editable)),
          EditorView.lineWrapping,
          theme_compartment_ref.current.of(create_editor_theme(properties.theme)),
          live_markdown_surface(properties.snapshot.document_id, properties.theme, properties.active, {
            on_save_requested: () => callbacks_ref.current.on_save_requested(properties.snapshot.document_id),
            on_source_mode_requested: () => callbacks_ref.current.on_source_mode_requested(),
            on_render_error: (message) => callbacks_ref.current.on_preview_error(message),
          }),
          EditorView.updateListener.of((update) => {
            if (update.docChanged) {
              const revision = callbacks_ref.current.on_document_change(
                properties.snapshot.document_id,
                update.state.doc.eq(baseline_document),
              );
              editor_revision_ref.current = revision;
              schedule_preview(revision, update.view);
            }
            if (update.docChanged || update.selectionSet) {
              const head = update.state.selection.main.head;
              const line = update.state.doc.lineAt(head);
              callbacks_ref.current.on_position_change(properties.snapshot.document_id, {
                line: line.number,
                column: head - line.from + 1,
              });
            }
          }),
        ],
      }),
    });
    const on_key_down = (event: KeyboardEvent) => {
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
        event.preventDefault();
        callbacks_ref.current.on_save_requested(properties.snapshot.document_id);
      } else if ((event.ctrlKey || event.metaKey) && !event.altKey && !event.shiftKey
          && (event.key === "/" || event.code === "Slash")) {
        event.preventDefault();
        callbacks_ref.current.on_source_mode_requested();
      }
    };
    view.dom.addEventListener("keydown", on_key_down);
    request_preview_ref.current = () => schedule_preview(editor_revision_ref.current, view);
    if (callbacks_ref.current.active) request_preview_ref.current();
    view_ref.current = view;
    const set_editable = (editable: boolean): void => {
      view.dispatch({
        effects: [
          editable_compartment_ref.current.reconfigure(EditorView.editable.of(editable)),
          read_only_compartment_ref.current.reconfigure(EditorState.readOnly.of(!editable)),
        ],
      });
      if (!editable) view.contentDOM.blur();
    };
    callbacks_ref.current.on_editor_handle_change(properties.snapshot.document_id, {
      undo: () => undo(view),
      redo: () => redo(view),
      set_editable,
      prepare_save: () => {
        const editor_revision = editor_revision_ref.current;
        save_documents.set(editor_revision, view.state.doc);
        return { content: view.state.doc.toString(), editor_revision };
      },
      accept_saved: (editor_revision) => {
        const saved_document = save_documents.get(editor_revision);
        if (!saved_document) throw new Error("保存修订对应的编辑器快照不存在");
        baseline_document = saved_document;
        for (const revision of save_documents.keys()) {
          if (revision <= editor_revision) save_documents.delete(revision);
        }
        return view.state.doc.eq(baseline_document);
      },
      reject_saved: (editor_revision) => {
        save_documents.delete(editor_revision);
      },
    });
    return () => {
      view.dom.removeEventListener("keydown", on_key_down);
      if (preview_timeout) clearTimeout(preview_timeout);
      request_preview_ref.current = null;
      callbacks_ref.current.on_editor_handle_change(properties.snapshot.document_id, null);
      view.destroy();
      view_ref.current = null;
    };
  }, [properties.snapshot.document_id]);

  useEffect(() => {
    const view = view_ref.current;
    if (!view) return;
    set_live_active(view, properties.active);
    if (properties.active) {
      view.focus();
      request_preview_ref.current?.();
    }
  }, [properties.active]);

  useEffect(() => {
    view_ref.current?.dispatch({
      effects: [
        editable_compartment_ref.current.reconfigure(EditorView.editable.of(properties.editable)),
        read_only_compartment_ref.current.reconfigure(EditorState.readOnly.of(!properties.editable)),
      ],
    });
    if (!properties.editable) view_ref.current?.contentDOM.blur();
  }, [properties.editable]);

  useEffect(() => {
    const view = view_ref.current;
    if (!view) return;
    const document = properties.preview_document;
    if (!document || document.revision === editor_revision_ref.current) {
      set_live_preview_document(view, document);
    }
  }, [properties.preview_document]);

  useEffect(() => {
    const view = view_ref.current;
    if (view) set_live_source_mode(view, properties.source_mode);
  }, [properties.source_mode]);

  useEffect(() => {
    const view = view_ref.current;
    if (!view) return;
    view.dispatch({ effects: theme_compartment_ref.current.reconfigure(create_editor_theme(properties.theme)) });
    set_live_theme(view, properties.theme);
  }, [properties.theme]);

  return (
    <section
      className={`editor_panel${properties.active ? " is_active" : ""}`}
      aria-label={`${properties.snapshot.name} 编辑器`}
      aria-hidden={!properties.active}
    >
      <div ref={host_ref} className="editor_host" />
    </section>
  );
}
