import { readFile } from "node:fs/promises";
import { join } from "node:path";
import type { Session } from "electron";
import { PREVIEW_CONTENT_SECURITY_POLICY, preview_asset_name } from "./preview_protocol_policy.mts";

const CONTENT_TYPES = Object.freeze({
  "index.html": "text/html; charset=utf-8",
  "runtime.js": "text/javascript; charset=utf-8",
  "styles.css": "text/css; charset=utf-8",
} satisfies Record<string, string>);

function response(status: number, body: BodyInit | null, content_type = "text/plain; charset=utf-8"): Response {
  return new Response(body, {
    status,
    headers: {
      "Content-Type": content_type,
      "Content-Security-Policy": PREVIEW_CONTENT_SECURITY_POLICY,
      "Cache-Control": "no-store",
      "X-Content-Type-Options": "nosniff",
      "Cross-Origin-Resource-Policy": "cross-origin",
      "Referrer-Policy": "no-referrer",
      "Access-Control-Allow-Origin": "*",
    },
  });
}

export function register_preview_protocol(workbench_session: Session, renderer_root: string): void {
  workbench_session.protocol.handle("loop-preview", async (request) => {
    if (request.method !== "GET") return response(405, "Method Not Allowed");
    const asset_name = preview_asset_name(request.url);
    if (!asset_name) return response(404, "Not Found");
    try {
      const contents = await readFile(join(renderer_root, "preview", asset_name));
      return response(200, contents, CONTENT_TYPES[asset_name as keyof typeof CONTENT_TYPES]);
    } catch {
      return response(404, "Not Found");
    }
  });
}
