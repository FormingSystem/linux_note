export interface document_session_state {
  document_id: string;
  editor_revision: number;
  dirty: boolean;
  saving_revision: number | null;
  last_saved_revision: number;
  save_status: "idle" | "saving" | "failed";
}

export function create_document_session(document_id: string): document_session_state {
  if (document_id.length === 0) throw new Error("document_id 不能为空");
  return {
    document_id,
    editor_revision: 0,
    dirty: false,
    saving_revision: null,
    last_saved_revision: 0,
    save_status: "idle",
  };
}

export function begin_document_save(
  state: document_session_state,
  editor_revision: number,
): document_session_state {
  if (!Number.isSafeInteger(editor_revision) || editor_revision < 0
      || editor_revision > state.editor_revision || state.saving_revision !== null) {
    throw new Error("保存修订无效");
  }
  return { ...state, saving_revision: editor_revision, save_status: "saving" };
}

export function complete_document_save(
  state: document_session_state,
  saved_revision: number,
  content_equals_saved_baseline: boolean,
): document_session_state {
  if (state.saving_revision !== saved_revision) throw new Error("保存响应修订无效");
  return {
    ...state,
    dirty: !content_equals_saved_baseline,
    saving_revision: null,
    last_saved_revision: saved_revision,
    save_status: "idle",
  };
}

export function fail_document_save(
  state: document_session_state,
  saved_revision: number,
): document_session_state {
  if (state.saving_revision !== saved_revision) return state;
  return { ...state, saving_revision: null, save_status: "failed" };
}

export function record_document_change(
  state: document_session_state,
  content_equals_baseline: boolean,
): document_session_state {
  if (!Number.isSafeInteger(state.editor_revision) || state.editor_revision < 0
      || state.editor_revision >= Number.MAX_SAFE_INTEGER) {
    throw new Error("editor_revision 无效");
  }
  return {
    ...state,
    editor_revision: state.editor_revision + 1,
    dirty: !content_equals_baseline,
  };
}
