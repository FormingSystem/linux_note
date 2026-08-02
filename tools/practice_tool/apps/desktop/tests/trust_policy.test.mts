import assert from "node:assert/strict";
import test from "node:test";
import type { IpcMainInvokeEvent } from "electron";
import { assert_trusted_ipc_sender, is_trusted_workbench_url } from "../src/main/security/trust_policy.mts";

function event_for(frame_url: string, main_frame_url = frame_url): IpcMainInvokeEvent {
  const main_frame = { url: main_frame_url };
  const sender_frame = frame_url === main_frame_url ? main_frame : { url: frame_url };
  return {
    sender: { mainFrame: main_frame },
    senderFrame: sender_frame,
  } as unknown as IpcMainInvokeEvent;
}

test("仅信任固定应用 origin", () => {
  assert.equal(is_trusted_workbench_url("loop-app://app/"), true);
  assert.equal(is_trusted_workbench_url("loop-app://other/"), false);
  assert.equal(is_trusted_workbench_url("https://example.com/"), false);
});

test("Main IPC 拒绝子 frame 与非应用页面", () => {
  assert.doesNotThrow(() => assert_trusted_ipc_sender(event_for("loop-app://app/")));
  assert.throws(
    () => assert_trusted_ipc_sender(event_for("loop-app://app/frame", "loop-app://app/")),
    /未授权/,
  );
  assert.throws(() => assert_trusted_ipc_sender(event_for("https://example.com/")), /未授权/);
});
