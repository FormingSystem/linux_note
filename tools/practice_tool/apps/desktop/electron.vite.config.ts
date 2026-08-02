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
    plugins: [react()],
    build: {
      minify: "esbuild",
      reportCompressedSize: true,
      sourcemap: true,
    },
  },
});
