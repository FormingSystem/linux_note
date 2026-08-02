import { TextDecoder } from "node:util";
import { MAX_NATIVE_CONTROL_FRAME_BYTES } from "@loop/ipc-contracts";

const HEADER_BYTES = 4;
const utf8Decoder = new TextDecoder("utf-8", { fatal: true });

export function encodeNativeFrame(value: unknown): Buffer {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  if (payload.byteLength === 0 || payload.byteLength > MAX_NATIVE_CONTROL_FRAME_BYTES) {
    throw new Error("Native Service 请求帧大小无效");
  }

  const frame = Buffer.allocUnsafe(HEADER_BYTES + payload.byteLength);
  frame.writeUInt32BE(payload.byteLength, 0);
  payload.copy(frame, HEADER_BYTES);
  return frame;
}

export class NativeFrameDecoder {
  private buffer = Buffer.alloc(0);

  push(chunk: Buffer): unknown[] {
    if (chunk.byteLength === 0) return [];
    this.buffer = this.buffer.byteLength === 0 ? chunk : Buffer.concat([this.buffer, chunk]);

    const frames: unknown[] = [];
    while (this.buffer.byteLength >= HEADER_BYTES) {
      const payloadLength = this.buffer.readUInt32BE(0);
      if (payloadLength === 0 || payloadLength > MAX_NATIVE_CONTROL_FRAME_BYTES) {
        throw new Error("Native Service 响应帧大小无效");
      }
      if (this.buffer.byteLength < HEADER_BYTES + payloadLength) break;

      const payload = this.buffer.subarray(HEADER_BYTES, HEADER_BYTES + payloadLength);
      this.buffer = this.buffer.subarray(HEADER_BYTES + payloadLength);
      frames.push(JSON.parse(utf8Decoder.decode(payload)) as unknown);
    }
    return frames;
  }
}
