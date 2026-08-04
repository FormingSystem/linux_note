import { copyFile, mkdir } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { build } from "esbuild";

const script_directory = dirname(fileURLToPath(import.meta.url));
const desktop_root = resolve(script_directory, "..");
const source_root = resolve(desktop_root, "src/renderer/src/preview_frame");
const output_root = resolve(desktop_root, "src/renderer/generated/preview");

await mkdir(output_root, { recursive: true });
await Promise.all([
  copyFile(resolve(source_root, "index.html"), resolve(output_root, "index.html")),
  copyFile(resolve(source_root, "styles.css"), resolve(output_root, "styles.css")),
  build({
    entryPoints: [resolve(source_root, "runtime.ts")],
    outfile: resolve(output_root, "runtime.js"),
    bundle: true,
    platform: "browser",
    format: "esm",
    target: "chrome140",
    minify: true,
    legalComments: "eof",
    sourcemap: false,
    logLevel: "warning",
  }),
]);
