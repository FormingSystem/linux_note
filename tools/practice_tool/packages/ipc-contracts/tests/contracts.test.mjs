import assert from "node:assert/strict";
import test from "node:test";
import {
  NATIVE_PROTOCOL_VERSION,
  isNativeResponseEnvelope,
  isRuntimeInfo,
} from "../dist/index.js";

const validResponse = Object.freeze({
  protocol_version: NATIVE_PROTOCOL_VERSION,
  request_id: "contract-test",
  ok: true,
  result: Object.freeze({
    service_name: "loop-native-service",
    service_version: "0.1.0",
    language: "C++",
  }),
});

test("接受字段完整且版本匹配的握手响应", () => {
  assert.equal(isNativeResponseEnvelope(validResponse), true);
});

test("拒绝跨语言协议中的未知字段", () => {
  assert.equal(isNativeResponseEnvelope({ ...validResponse, path: "C:/not-allowed" }), false);
  assert.equal(isNativeResponseEnvelope({
    ...validResponse,
    result: { ...validResponse.result, pointer: "0x1234" },
  }), false);
});

test("拒绝版本不匹配和不完整错误", () => {
  assert.equal(isNativeResponseEnvelope({ ...validResponse, protocol_version: 2 }), false);
  assert.equal(isNativeResponseEnvelope({
    protocol_version: NATIVE_PROTOCOL_VERSION,
    request_id: "contract-test",
    ok: false,
    error: { code: "FAILED", message: "" },
  }), false);
});

test("Preload 拒绝夹带未知字段的运行状态", () => {
  const runtime = {
    appName: "Loop",
    appVersion: "0.1.0",
    platform: "win32",
    electronVersion: "43.2.0",
    nativeService: {
      status: "ready",
      protocolVersion: NATIVE_PROTOCOL_VERSION,
      serviceVersion: "0.1.0",
      message: "ready",
    },
  };
  assert.equal(isRuntimeInfo(runtime), true);
  assert.equal(isRuntimeInfo({ ...runtime, absolutePath: "C:/not-allowed" }), false);
});
