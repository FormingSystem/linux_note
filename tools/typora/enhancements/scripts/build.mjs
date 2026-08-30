import { build } from "esbuild";

await build({
  entryPoints: ["src/typora_enhancements.ts"],
  bundle: true,
  format: "iife",
  globalName: "LinuxNoteTyporaEnhancements",
  platform: "browser",
  target: ["chrome120"],
  outfile: "dist/typora_enhancements.js",
  legalComments: "linked",
  loader: {
    ".css": "text",
    ".wasm": "binary"
  },
  logLevel: "info"
});
