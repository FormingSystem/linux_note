import assert from "node:assert/strict";
import test from "node:test";
import { MAX_NATIVE_CONTROL_FRAME_BYTES } from "@loop/ipc-contracts";
import { encode_native_frame, native_frame_decoder } from "../src/main/native_service/framing.mts";

test("换行分帧可跨数据块恢复 UTF-8 JSON", () => {
  const decoder = new native_frame_decoder();
  const frame = encode_native_frame({ message: "目录可浏览" });
  const split = frame.indexOf(Buffer.from("可"));

  assert.deepEqual(decoder.push(frame.subarray(0, split + 1)), []);
  assert.deepEqual(decoder.push(frame.subarray(split + 1)), [{ message: "目录可浏览" }]);
});

test("解码器接受 Windows 文本流的 CRLF 帧边界", () => {
  const decoder = new native_frame_decoder();
  assert.deepEqual(decoder.push(Buffer.from('{"ok":true}\r\n')), [{ ok: true }]);
});

test("解码器在换行前拒绝超限帧", () => {
  const decoder = new native_frame_decoder();
  const oversized = Buffer.alloc(MAX_NATIVE_CONTROL_FRAME_BYTES + 2, 0x78);
  assert.throws(() => decoder.push(oversized), /帧大小无效/);
});
