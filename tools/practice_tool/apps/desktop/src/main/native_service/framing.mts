import { TextDecoder } from "node:util";
import { MAX_NATIVE_CONTROL_FRAME_BYTES } from "@loop/ipc-contracts";

const utf8Decoder = new TextDecoder("utf-8", { fatal: true });

export function encode_native_frame(value: unknown): Buffer {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  if (payload.byteLength === 0 || payload.byteLength > MAX_NATIVE_CONTROL_FRAME_BYTES) {
    throw new Error("Native Service 请求帧大小无效");
  }

  return Buffer.concat([payload, Buffer.from("\n")]);
}

export class native_frame_decoder {
  private buffer = Buffer.alloc(0);

  push(chunk: Buffer): unknown[] {
    if (chunk.byteLength === 0) return [];
    this.buffer = this.buffer.byteLength === 0 ? chunk : Buffer.concat([this.buffer, chunk]);

    const frames: unknown[] = [];
    while (true) {
      const line_end = this.buffer.indexOf(0x0a);
      if (line_end < 0) {
        if (this.buffer.byteLength > MAX_NATIVE_CONTROL_FRAME_BYTES + 1) {
          throw new Error("Native Service 响应帧大小无效");
        }
        break;
      }
      let payload_length = line_end;
      if (payload_length > 0 && this.buffer[payload_length - 1] === 0x0d) payload_length -= 1;
      if (payload_length === 0 || payload_length > MAX_NATIVE_CONTROL_FRAME_BYTES) {
        throw new Error("Native Service 响应帧大小无效");
      }
      const payload = this.buffer.subarray(0, payload_length);
      this.buffer = this.buffer.subarray(line_end + 1);
      frames.push(JSON.parse(utf8Decoder.decode(payload)) as unknown);
    }
    return frames;
  }
}
