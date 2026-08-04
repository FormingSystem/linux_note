import assert from "node:assert/strict";
import test from "node:test";
import {
  begin_document_save,
  complete_document_save,
  create_document_session,
  fail_document_save,
  record_document_change,
} from "../dist/index.js";

test("正文修改递增修订并进入 Dirty", () => {
  const initial = create_document_session("document_0123456789abcdef0123456789abcdef");
  const changed = record_document_change(initial, false);
  assert.equal(changed.editor_revision, 1);
  assert.equal(changed.dirty, true);
});

test("完整撤销回加载基线恢复 Clean 且修订继续递增", () => {
  const initial = create_document_session("document_0123456789abcdef0123456789abcdef");
  const changed = record_document_change(initial, false);
  const undone = record_document_change(changed, true);
  assert.equal(undone.editor_revision, 2);
  assert.equal(undone.dirty, false);
});

test("修订号达到安全整数上限时失败关闭", () => {
  assert.throws(() => record_document_change({
    document_id: "document_0123456789abcdef0123456789abcdef",
    editor_revision: Number.MAX_SAFE_INTEGER,
    dirty: true,
  }, false), /editor_revision/);
});

test("保存旧修订期间继续编辑时仍保持 Dirty", () => {
  let state = record_document_change(create_document_session("document_0123456789abcdef0123456789abcdef"), false);
  state = begin_document_save(state, 1);
  state = record_document_change(state, false);
  state = complete_document_save(state, 1, false);
  assert.equal(state.editor_revision, 2);
  assert.equal(state.last_saved_revision, 1);
  assert.equal(state.dirty, true);
  assert.equal(state.save_status, "idle");
});

test("保存失败只结束匹配修订且不清除 Dirty", () => {
  let state = record_document_change(create_document_session("document_0123456789abcdef0123456789abcdef"), false);
  state = begin_document_save(state, 1);
  assert.equal(fail_document_save(state, 0), state);
  state = fail_document_save(state, 1);
  assert.equal(state.dirty, true);
  assert.equal(state.saving_revision, null);
  assert.equal(state.save_status, "failed");
});
