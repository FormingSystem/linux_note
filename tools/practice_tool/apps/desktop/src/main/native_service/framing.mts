import { TextDecoder } from "node:util";
import {
  MAX_NATIVE_BODY_FRAME_BYTES,
  MAX_NATIVE_CONTROL_FRAME_BYTES,
} from "@loop/ipc-contracts";

const FRAME_HEADER_BYTES = 16;
const FRAME_VERSION = 1;
const BODY_PRESENT_FLAG = 0x01;
const FRAME_MAGIC = Buffer.from("LOOP", "ascii");
const utf8_decoder = new TextDecoder("utf-8", { fatal: true });

export interface native_transport_frame {
  control: unknown;
  body: Buffer;
  body_present: boolean;
}

export function encode_native_frame(
  control: unknown,
  body: Buffer = Buffer.alloc(0),
  body_present = false,
): Buffer {
  const control_bytes = Buffer.from(JSON.stringify(control), "utf8");
  if (control_bytes.byteLength === 0 || control_bytes.byteLength > MAX_NATIVE_CONTROL_FRAME_BYTES) {
    throw new Error("Native Service 控制区大小无效");
  }
  if (body.byteLength > MAX_NATIVE_BODY_FRAME_BYTES || (!body_present && body.byteLength !== 0)) {
    throw new Error("Native Service 正文附件大小无效");
  }

  const header = Buffer.alloc(FRAME_HEADER_BYTES);
  FRAME_MAGIC.copy(header, 0);
  header[4] = FRAME_VERSION;
  header[5] = body_present ? BODY_PRESENT_FLAG : 0;
  header.writeUInt32BE(control_bytes.byteLength, 8);
  header.writeUInt32BE(body.byteLength, 12);
  return Buffer.concat([header, control_bytes, body]);
}

export class native_frame_decoder {
  private buffer = Buffer.alloc(0);
  private start_offset = 0;
  private end_offset = 0;

  push(chunk: Buffer): native_transport_frame[] {
    const frames: native_transport_frame[] = [];
    const maximum_buffer = FRAME_HEADER_BYTES + MAX_NATIVE_CONTROL_FRAME_BYTES + MAX_NATIVE_BODY_FRAME_BYTES;
    let chunk_offset = 0;
    while (chunk_offset < chunk.byteLength) {
      const buffered_bytes = this.end_offset - this.start_offset;
      const available_bytes = maximum_buffer - buffered_bytes;
      if (available_bytes === 0) throw new Error("Native Service 帧缓冲区超过上限");
      const copied_bytes = Math.min(available_bytes, chunk.byteLength - chunk_offset);
      this.append(chunk.subarray(chunk_offset, chunk_offset + copied_bytes), maximum_buffer);
      chunk_offset += copied_bytes;
      this.decode_available(frames);
    }
    this.decode_available(frames);
    return frames;
  }

  finish(): void {
    if (this.end_offset !== this.start_offset) throw new Error("Native Service 输出包含截断帧");
  }

  private append(chunk: Buffer, maximum_buffer: number): void {
    if (chunk.byteLength === 0) return;
    const buffered_bytes = this.end_offset - this.start_offset;
    if (this.buffer.byteLength - this.end_offset < chunk.byteLength && this.start_offset > 0
        && this.buffer.byteLength - buffered_bytes >= chunk.byteLength) {
      this.buffer.copy(this.buffer, 0, this.start_offset, this.end_offset);
      this.start_offset = 0;
      this.end_offset = buffered_bytes;
    }
    if (this.buffer.byteLength - this.end_offset < chunk.byteLength) {
      const required_bytes = buffered_bytes + chunk.byteLength;
      const next_capacity = Math.min(
        maximum_buffer,
        Math.max(required_bytes, Math.max(1024, this.buffer.byteLength * 2)),
      );
      const next = Buffer.allocUnsafe(next_capacity);
      this.buffer.copy(next, 0, this.start_offset, this.end_offset);
      this.buffer = next;
      this.start_offset = 0;
      this.end_offset = buffered_bytes;
    }
    chunk.copy(this.buffer, this.end_offset);
    this.end_offset += chunk.byteLength;
  }

  private decode_available(frames: native_transport_frame[]): void {
    while (this.end_offset - this.start_offset >= FRAME_HEADER_BYTES) {
      const header = this.buffer.subarray(this.start_offset, this.start_offset + FRAME_HEADER_BYTES);
      if (!header.subarray(0, 4).equals(FRAME_MAGIC)) throw new Error("Native Service 帧魔数无效");
      const version = header[4] ?? -1;
      const flags = header[5] ?? -1;
      if (version !== FRAME_VERSION || (flags & ~BODY_PRESENT_FLAG) !== 0 || header[6] !== 0 || header[7] !== 0) {
        throw new Error("Native Service 帧头无效");
      }
      const body_present = (flags & BODY_PRESENT_FLAG) !== 0;
      const control_length = header.readUInt32BE(8);
      const body_length = header.readUInt32BE(12);
      if (control_length === 0 || control_length > MAX_NATIVE_CONTROL_FRAME_BYTES) {
        throw new Error("Native Service 控制区大小无效");
      }
      if (body_length > MAX_NATIVE_BODY_FRAME_BYTES || (!body_present && body_length !== 0)) {
        throw new Error("Native Service 正文附件大小无效");
      }
      const total_length = FRAME_HEADER_BYTES + control_length + body_length;
      if (this.end_offset - this.start_offset < total_length) return;

      const control_start = this.start_offset + FRAME_HEADER_BYTES;
      const body_start = control_start + control_length;
      const frame_end = this.start_offset + total_length;
      let control: unknown;
      try {
        control = JSON.parse(utf8_decoder.decode(this.buffer.subarray(control_start, body_start))) as unknown;
      } catch {
        throw new Error("Native Service 控制区不是有效的 UTF-8 JSON");
      }
      frames.push({
        control,
        body: Buffer.from(this.buffer.subarray(body_start, frame_end)),
        body_present,
      });
      this.start_offset = frame_end;
      if (this.start_offset === this.end_offset) {
        this.start_offset = 0;
        this.end_offset = 0;
      }
    }
  }
}
