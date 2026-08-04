import { resolve } from "node:path";
import react from "@vitejs/plugin-react";
import { defineConfig } from "electron-vite";

export default defineConfig({
  main: {
    build: {
      externalizeDeps: false,
      sourcemap: true,
    },
  },
  preload: {
    build: {
      externalizeDeps: false,
      sourcemap: true,
    },
  },
  renderer: {
    root: resolve("src/renderer"),
    publicDir: resolve("src/renderer/generated"),
    resolve: {
      alias: {
        // Unified 的浏览器条件实现依赖 DOM；Worker 必须固定到无 DOM 的纯数据实现。
        "decode-named-character-reference": resolve("../../node_modules/decode-named-character-reference/index.js"),
      },
    },
    plugins: [react()],
    build: {
      minify: "esbuild",
      reportCompressedSize: true,
      sourcemap: true,
    },
  },
});
