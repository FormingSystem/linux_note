import { createHash } from "node:crypto";
import {
  is_native_response_envelope,
  type native_method,
  type native_response_envelope,
} from "@loop/ipc-contracts";
import type { native_transport_frame } from "./framing.mts";

export interface validated_native_response {
  envelope: native_response_envelope;
  body: Buffer;
}

export function validate_native_response_frame(
  frame: native_transport_frame,
  method: native_method | null,
): validated_native_response {
  if (!is_native_response_envelope(frame.control)) throw new Error("Native Service 响应无效");
  const envelope = frame.control;
  if (envelope.body === null) {
    if (frame.body_present || frame.body.byteLength !== 0) throw new Error("Native Service 返回了未声明正文");
  } else {
    if (!frame.body_present
        || frame.body.byteLength !== envelope.body.byte_length
        || createHash("sha256").update(frame.body).digest("hex") !== envelope.body.sha256) {
      throw new Error("Native Service 正文描述与附件不一致");
    }
    if (method !== null && method !== "workspace.open_document") {
      throw new Error("Native Service 在非正文方法中返回了附件");
    }
  }
  return { envelope, body: frame.body };
}
