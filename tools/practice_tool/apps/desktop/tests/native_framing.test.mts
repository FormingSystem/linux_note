import assert from "node:assert/strict";
import test from "node:test";
import { MAX_NATIVE_BODY_FRAME_BYTES, MAX_NATIVE_CONTROL_FRAME_BYTES } from "@loop/ipc-contracts";
import { encode_native_frame, native_frame_decoder } from "../src/main/native_service/framing.mts";
import { validate_native_response_frame } from "../src/main/native_service/response_validation.mts";

test("复合帧在任意单字节分块后恢复控制区与正文", () => {
  const decoder = new native_frame_decoder();
  const body = Buffer.from("# 目录\n正文", "utf8");
  const frame = encode_native_frame({ message: "目录可浏览" }, body, true);
  const decoded = [];
  for (const byte of frame) decoded.push(...decoder.push(Buffer.from([byte])));
  assert.equal(decoded.length, 1);
  assert.deepEqual(decoded[0]?.control, { message: "目录可浏览" });
  assert.equal(decoded[0]?.body_present, true);
  assert.deepEqual(decoded[0]?.body, body);
  decoder.finish();
});

test("空正文以附件存在且长度为零往返", () => {
  const decoder = new native_frame_decoder();
  const [decoded] = decoder.push(encode_native_frame({ ok: true }, Buffer.alloc(0), true));
  assert.equal(decoded?.body_present, true);
  assert.equal(decoded?.body.byteLength, 0);
});

test("同一数据块可携带多帧", () => {
  const decoder = new native_frame_decoder();
  const frames = decoder.push(Buffer.concat([
    encode_native_frame({ index: 1 }),
    encode_native_frame({ index: 2 }, Buffer.from("x"), true),
  ]));
  assert.deepEqual(frames.map((frame) => frame.control), [{ index: 1 }, { index: 2 }]);
});

test("接受精确 1 MiB 控制区和 5 MiB 正文边界", () => {
  const empty_control_length = Buffer.byteLength(JSON.stringify({ value: "" }));
  const value = "x".repeat(MAX_NATIVE_CONTROL_FRAME_BYTES - empty_control_length);
  const body = Buffer.alloc(MAX_NATIVE_BODY_FRAME_BYTES, 0x61);
  const frame = encode_native_frame({ value }, body, true);
  const decoder = new native_frame_decoder();
  const decoded = [];
  for (let offset = 0; offset < frame.byteLength; offset += 8191) {
    decoded.push(...decoder.push(frame.subarray(offset, Math.min(frame.byteLength, offset + 8191))));
  }
  assert.equal(decoded[0]?.body.byteLength, MAX_NATIVE_BODY_FRAME_BYTES);
  assert.equal((decoded[0]?.control as { value: string }).value.length, value.length);
});

test("编码器拒绝超限控制区、正文和未声明正文", () => {
  assert.throws(() => encode_native_frame({ value: "x".repeat(MAX_NATIVE_CONTROL_FRAME_BYTES) }), /控制区/);
  assert.throws(() => encode_native_frame({}, Buffer.alloc(MAX_NATIVE_BODY_FRAME_BYTES + 1), true), /正文附件/);
  assert.throws(() => encode_native_frame({}, Buffer.from("x"), false), /正文附件/);
});

test("解码器拒绝错误魔数、版本、标志和保留位", () => {
  const mutations: Array<readonly [number, number]> = [[0, 0x58], [4, 2], [5, 2], [6, 1], [7, 1]];
  for (const [offset, value] of mutations) {
    const frame = encode_native_frame({ ok: true });
    frame[offset] = value;
    assert.throws(() => new native_frame_decoder().push(frame), /魔数|帧头/);
  }
});

test("解码器拒绝无效 UTF-8、JSON、长度和截断 EOF", () => {
  const invalid_utf8 = encode_native_frame({ ok: true });
  invalid_utf8[16] = 0xff;
  assert.throws(() => new native_frame_decoder().push(invalid_utf8), /UTF-8 JSON/);

  const invalid_json = encode_native_frame({ ok: true });
  invalid_json[16] = 0x78;
  assert.throws(() => new native_frame_decoder().push(invalid_json), /UTF-8 JSON/);

  const invalid_length = encode_native_frame({ ok: true });
  invalid_length.writeUInt32BE(MAX_NATIVE_CONTROL_FRAME_BYTES + 1, 8);
  assert.throws(() => new native_frame_decoder().push(invalid_length), /控制区/);

  const decoder = new native_frame_decoder();
  decoder.push(encode_native_frame({ ok: true }).subarray(0, -1));
  assert.throws(() => decoder.finish(), /截断帧/);
});

test("响应附件描述、摘要、方法与错误帧规则严格一致", () => {
  const body = Buffer.from("hello");
  const descriptor = {
    kind: "markdown_utf8",
    byte_length: 5,
    sha256: "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
  } as const;
  const control = {
    protocol_version: 4,
    request_id: "document-response",
    ok: true,
    body: descriptor,
    result: {},
  };
  assert.deepEqual(
    validate_native_response_frame({ control, body, body_present: true }, "workspace.open_document").body,
    body,
  );
  assert.throws(() => validate_native_response_frame(
    { control: { ...control, body: { ...descriptor, byte_length: 4 } }, body, body_present: true },
    "workspace.open_document",
  ), /描述与附件/);
  assert.throws(() => validate_native_response_frame(
    { control: { ...control, body: { ...descriptor, sha256: "0".repeat(64) } }, body, body_present: true },
    "workspace.open_document",
  ), /描述与附件/);
  assert.throws(() => validate_native_response_frame(
    { control, body, body_present: true },
    "system.handshake",
  ), /非正文方法/);
  assert.throws(() => validate_native_response_frame({
    control: {
      protocol_version: 4,
      request_id: "error-response",
      ok: false,
      body: null,
      error: {
        code: "INVALID_BODY",
        user_message: "invalid",
        retryable: false,
        recovery_actions: [],
        correlation_id: "error-response",
      },
    },
    body,
    body_present: true,
  }, null), /未声明正文/);
});
