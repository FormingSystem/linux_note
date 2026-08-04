import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawn } from "node:child_process";
import { encode_native_frame, native_frame_decoder } from "../src/main/native_service/framing.mts";
import { validate_native_response_frame } from "../src/main/native_service/response_validation.mts";
import { NATIVE_PROTOCOL_VERSION } from "@loop/ipc-contracts";

const maximum_body_bytes = 5 * 1024 * 1024;
const script_directory = dirname(fileURLToPath(import.meta.url));
const preset = process.platform === "win32" ? "windows-mingw" : "linux-gcc";
const executable_name = process.platform === "win32" ? "loop_native_service.exe" : "loop_native_service";
const executable = resolve(script_directory, "../../../native/build", preset, executable_name);

function wait_for_exit(child) {
  return new Promise((resolve_exit) => child.once("exit", (code, signal) => resolve_exit({ code, signal })));
}

function system_only_path() {
  if (process.platform !== "win32") return process.env.PATH ?? "";
  const system_root = process.env.SystemRoot ?? "C:\\Windows";
  return [join(system_root, "System32"), system_root].join(";");
}

function start_client() {
  const child = spawn(executable, ["--stdio"], {
    cwd: dirname(executable),
    shell: false,
    windowsHide: true,
    stdio: ["pipe", "pipe", "pipe"],
  });
  const decoder = new native_frame_decoder();
  const pending = new Map();
  let request_sequence = 0;
  child.stderr.resume();
  child.stdout.on("data", (chunk) => {
    try {
      for (const frame of decoder.push(chunk)) {
        const request_id = typeof frame.control === "object" && frame.control !== null
          && "request_id" in frame.control ? String(frame.control.request_id) : "";
        const request = pending.get(request_id);
        if (!request) throw new Error("UNEXPECTED_NATIVE_RESPONSE");
        pending.delete(request_id);
        clearTimeout(request.timer);
        request.resolve(validate_native_response_frame(frame, request.method));
      }
    } catch (error) {
      for (const request of pending.values()) request.reject(error);
      pending.clear();
      child.kill();
    }
  });
  child.once("exit", () => {
    for (const request of pending.values()) request.reject(new Error("NATIVE_EXITED"));
    pending.clear();
  });

  return {
    child,
    request(method, params, body) {
      const request_id = `transport-smoke-${++request_sequence}`;
      const descriptor = body === undefined ? null : {
        kind: "markdown_source_utf8",
        byte_length: body.byteLength,
        sha256: createHash("sha256").update(body).digest("hex"),
      };
      const frame = encode_native_frame(
        { protocol_version: NATIVE_PROTOCOL_VERSION, request_id, method, params, body: descriptor },
        body,
        body !== undefined,
      );
      return new Promise((resolve_request, reject_request) => {
        const timer = setTimeout(() => {
          pending.delete(request_id);
          reject_request(new Error("NATIVE_RESPONSE_TIMEOUT"));
        }, 20_000);
        timer.unref();
        pending.set(request_id, { method, resolve: resolve_request, reject: reject_request, timer });
        child.stdin.write(frame, (error) => {
          if (!error) return;
          clearTimeout(timer);
          pending.delete(request_id);
          reject_request(error);
        });
      });
    },
  };
}

const fixture_directory = await mkdtemp(join(tmpdir(), "loop-native-transport-"));
let client = null;
try {
  const fixture_path = join(fixture_directory, "exact-limit.md");
  const fixture_body = Buffer.alloc(maximum_body_bytes, 0x78);
  await writeFile(fixture_path, fixture_body);

  client = start_client();
  const window_session_id = "window_native_transport_smoke";
  const handshake = await client.request("system.handshake", {
    client_name: "loop_desktop",
    client_version: "0.1.0",
  });
  assert.equal(handshake.envelope.ok, true);

  const opened = await client.request("workspace.open_file", { window_session_id, locator: fixture_path });
  assert.equal(opened.envelope.ok, true);
  const opened_result = opened.envelope.result;
  assert.equal(typeof opened_result, "object");
  assert.ok(opened_result && "workspace_id" in opened_result && "document" in opened_result);
  const document = opened_result.document;
  assert.ok(document && typeof document === "object" && "document_id" in document);

  const snapshot = await client.request("workspace.open_document", {
    window_session_id,
    workspace_id: opened_result.workspace_id,
    target_kind: "document",
    target_id: document.document_id,
  });
  assert.equal(snapshot.envelope.ok, true);
  assert.equal(snapshot.body.byteLength, maximum_body_bytes);
  assert.equal(snapshot.envelope.body?.byte_length, maximum_body_bytes);
  assert.equal(snapshot.envelope.body?.sha256, createHash("sha256").update(fixture_body).digest("hex"));
  assert.equal(snapshot.envelope.result.content_hash, snapshot.envelope.body?.sha256);
  assert.equal("locator" in snapshot.envelope.result, false);
  assert.equal("absolute_path" in snapshot.envelope.result, false);

  const saved = await client.request("workspace.save_document", {
    window_session_id,
    workspace_id: opened_result.workspace_id,
    document_id: snapshot.envelope.result.document_id,
    expected_file_version_token: snapshot.envelope.result.file_version_token,
    expected_content_hash: snapshot.envelope.result.content_hash,
    editor_revision: 1,
    line_ending_policy: "preserve",
  }, fixture_body);
  assert.equal(saved.envelope.ok, true);
  assert.equal(saved.envelope.body, null);
  assert.equal(saved.envelope.result.saved_revision, 1);

  await client.request("workspace.close", { window_session_id });
  const normal_exit = wait_for_exit(client.child);
  client.child.stdin.end();
  assert.deepEqual(await normal_exit, { code: 0, signal: null });
  client = null;

  const isolated_runtime = spawn(executable, ["--stdio"], {
    cwd: dirname(executable),
    env: {
      ...process.env,
      PATH: system_only_path(),
      Path: system_only_path(),
    },
    shell: false,
    windowsHide: true,
    stdio: ["pipe", "pipe", "pipe"],
  });
  isolated_runtime.stdout.resume();
  isolated_runtime.stderr.resume();
  const isolated_exit = wait_for_exit(isolated_runtime);
  isolated_runtime.stdin.end();
  assert.deepEqual(await isolated_exit, { code: 0, signal: null });

  const truncated = spawn(executable, ["--stdio"], {
    cwd: dirname(executable),
    shell: false,
    windowsHide: true,
    stdio: ["pipe", "pipe", "pipe"],
  });
  truncated.stdout.resume();
  truncated.stderr.resume();
  const truncated_exit = wait_for_exit(truncated);
  const complete_frame = encode_native_frame({
    protocol_version: NATIVE_PROTOCOL_VERSION,
    request_id: "truncated-smoke",
    method: "system.handshake",
    params: { client_name: "loop_desktop", client_version: "0.1.0" },
    body: null,
  });
  truncated.stdin.end(complete_frame.subarray(0, 15));
  const abnormal_exit = await truncated_exit;
  assert.notEqual(abnormal_exit.code, 0);

  console.log(JSON.stringify({
    executable: basename(executable),
    exact_body_bytes: snapshot.body.byteLength,
    normal_exit: 0,
    isolated_runtime_exit: 0,
    truncated_exit: abnormal_exit.code,
  }));
} finally {
  client?.child.kill();
  await rm(fixture_directory, { recursive: true, force: true });
}
