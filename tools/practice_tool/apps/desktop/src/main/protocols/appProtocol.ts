import { readFile } from "node:fs/promises";
import { extname, isAbsolute, relative, resolve } from "node:path";
import type { Session } from "electron";

const CONTENT_SECURITY_POLICY = [
  "default-src 'none'",
  "script-src 'self'",
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' data:",
  "font-src 'self'",
  "connect-src 'self'",
  "base-uri 'none'",
  "form-action 'none'",
  "frame-ancestors 'none'",
].join("; ");

const MIME_TYPES: Readonly<Record<string, string>> = Object.freeze({
  ".css": "text/css; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".map": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".woff2": "font/woff2",
});

function response(status: number, body: BodyInit | null, contentType = "text/plain; charset=utf-8"): Response {
  return new Response(body, {
    status,
    headers: {
      "Content-Type": contentType,
      "Content-Security-Policy": CONTENT_SECURITY_POLICY,
      "Cache-Control": "no-store",
      "X-Content-Type-Options": "nosniff",
    },
  });
}

function resolveAsset(rendererRoot: string, requestUrl: string): string | null {
  const url = new URL(requestUrl);
  if (url.protocol !== "loop-app:" || url.hostname !== "app" || url.username || url.password || url.port
      || url.search || url.hash) return null;

  let pathname: string;
  try {
    pathname = decodeURIComponent(url.pathname);
  } catch {
    return null;
  }
  if (pathname.includes("\0") || pathname.includes("\\")) return null;

  const asset = pathname === "/" ? "index.html" : pathname.replace(/^\/+/, "");
  const target = resolve(rendererRoot, asset);
  const relation = relative(rendererRoot, target);
  if (!relation || relation.startsWith("..") || isAbsolute(relation)) return relation ? null : target;
  return target;
}

export function registerAppProtocol(session: Session, rendererRoot: string): void {
  session.protocol.handle("loop-app", async (request) => {
    if (request.method !== "GET") return response(405, "Method Not Allowed");
    const target = resolveAsset(rendererRoot, request.url);
    if (!target) return response(404, "Not Found");

    try {
      const contents = await readFile(target);
      return response(200, contents, MIME_TYPES[extname(target).toLowerCase()] ?? "application/octet-stream");
    } catch {
      return response(404, "Not Found");
    }
  });
}
