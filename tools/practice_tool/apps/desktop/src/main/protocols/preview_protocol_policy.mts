export const PREVIEW_CONTENT_SECURITY_POLICY = [
  "default-src 'none'",
  "script-src loop-preview:",
  "style-src loop-preview: 'unsafe-inline'",
  "img-src data:",
  "font-src 'none'",
  "connect-src 'none'",
  "object-src 'none'",
  "base-uri 'none'",
  "form-action 'none'",
  "frame-ancestors loop-app://app http://127.0.0.1:* http://localhost:*",
].join("; ");

export const PREVIEW_SCHEME_PRIVILEGES = Object.freeze({
  standard: true,
  secure: true,
  codeCache: true,
  corsEnabled: true,
});

const PREVIEW_ASSETS = new Map<string, string>([
  ["/", "index.html"],
  ["/index.html", "index.html"],
  ["/runtime.js", "runtime.js"],
  ["/styles.css", "styles.css"],
]);

export function preview_asset_name(request_url: string): string | null {
  if (request_url.includes("%") || /\/\.\.?(?:\/|$)/u.test(request_url)) return null;
  let url: URL;
  try {
    url = new URL(request_url);
  } catch {
    return null;
  }
  if (url.protocol !== "loop-preview:" || url.hostname !== "preview" || url.username || url.password
      || url.port || url.search || url.hash) return null;
  let pathname: string;
  try {
    pathname = decodeURIComponent(url.pathname);
  } catch {
    return null;
  }
  if (pathname.includes("\0") || pathname.includes("\\")) return null;
  return PREVIEW_ASSETS.get(pathname) ?? null;
}
