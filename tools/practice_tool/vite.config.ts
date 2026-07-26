import fs from "node:fs";
import path from "node:path";
import { defineConfig, type Plugin } from "vite";
import react from "@vitejs/plugin-react";

type KnowledgeSourceRegistry = {
  schema_version: number;
  sources: Array<{
    id: string;
    title: string;
    kind: "filesystem" | "http";
    location: string;
  }>;
};

async function loadKnowledgeSources(): Promise<KnowledgeSourceRegistry & { config_source: string | null }> {
  const configured = process.env.PRACTICE_SOURCE_CONFIG;
  const localDefault = path.resolve(process.cwd(), "config", "knowledge_sources.local.json");
  const address = configured || (fs.existsSync(localDefault) ? localDefault : "");

  if (!address) return { schema_version: 1, config_source: null, sources: [] };

  let raw: string;
  let base: string;
  const remoteConfig = /^https?:\/\//i.test(address);
  if (remoteConfig) {
    const response = await fetch(address);
    if (!response.ok) throw new Error(`知识源配置读取失败：${response.status} ${address}`);
    raw = await response.text();
    base = address;
  } else {
    const absolute = path.resolve(address);
    raw = fs.readFileSync(absolute, "utf8");
    base = path.dirname(absolute);
  }

  const registry = JSON.parse(raw) as KnowledgeSourceRegistry;
  if (registry.schema_version !== 1 || !Array.isArray(registry.sources)) {
    throw new Error(`知识源配置格式无效：${address}`);
  }

  const sources = registry.sources.map((source) => {
    if (!source.id || !source.title || !["filesystem", "http"].includes(source.kind) || !source.location) {
      throw new Error(`知识源配置字段不完整：${address}`);
    }
    if (remoteConfig && source.kind === "filesystem") {
      throw new Error(`远程知识源配置不能声明本机文件系统地址：${source.id}`);
    }
    const location = source.kind === "filesystem"
      ? path.resolve(base, source.location)
      : new URL(source.location, base).toString();
    return { ...source, location };
  });

  return { schema_version: 1, config_source: address, sources };
}

function runtimeConfigPlugin(runtimeConfig: Awaited<ReturnType<typeof loadKnowledgeSources>>): Plugin {
  const virtualId = "virtual:practice-runtime-config";
  const resolvedId = `\0${virtualId}`;
  return {
    name: "practice-runtime-config",
    resolveId(id) {
      return id === virtualId ? resolvedId : undefined;
    },
    load(id) {
      return id === resolvedId ? `export default ${JSON.stringify(runtimeConfig)};` : undefined;
    },
  };
}

export default defineConfig(async () => {
  const runtimeConfig = await loadKnowledgeSources();
  return {
    plugins: [react(), runtimeConfigPlugin(runtimeConfig)],
    base: "./",
  };
});
