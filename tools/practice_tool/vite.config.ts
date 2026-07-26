import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { defineConfig, type Plugin } from "vite";
import react from "@vitejs/plugin-react";

const execFileAsync = promisify(execFile);

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

type UpdateState = {
  status: "idle" | "checking" | "current" | "available" | "error" | "updated";
  checked_at: string | null;
  current_commit: string | null;
  remote_commit: string | null;
  behind_count: number;
  message: string;
};

function readJson(file: string) {
  return JSON.parse(fs.readFileSync(file, "utf8")) as Record<string, unknown>;
}

async function git(repoRoot: string, args: string[]) {
  const result = await execFileAsync("git", ["-C", repoRoot, ...args], {
    windowsHide: true,
    timeout: 120_000,
    maxBuffer: 1024 * 1024,
  });
  return result.stdout.trim();
}

function systemServicePlugin(toolRoot: string, release: Record<string, unknown>, security: Record<string, unknown>, token: string): Plugin {
  let state: UpdateState = {
    status: "idle",
    checked_at: null,
    current_commit: null,
    remote_commit: null,
    behind_count: 0,
    message: "尚未检查更新",
  };
  let checking: Promise<UpdateState> | null = null;

  const check = async () => {
    if (checking) return checking;
    checking = (async () => {
      state = { ...state, status: "checking", message: "正在后台检查更新" };
      try {
        const repoRoot = await git(toolRoot, ["rev-parse", "--show-toplevel"]);
        const branch = String(release.repository_branch || "master");
        await git(repoRoot, ["fetch", "--quiet", "origin", branch]);
        const current = await git(repoRoot, ["rev-parse", "HEAD"]);
        const remote = await git(repoRoot, ["rev-parse", `origin/${branch}`]);
        const behind = Number(await git(repoRoot, ["rev-list", "--count", `HEAD..origin/${branch}`]));
        state = {
          status: behind > 0 ? "available" : "current",
          checked_at: new Date().toISOString(),
          current_commit: current,
          remote_commit: remote,
          behind_count: behind,
          message: behind > 0 ? `发现 ${behind} 个新提交` : "当前已是最新版本",
        };
      } catch (error) {
        state = {
          ...state,
          status: "error",
          checked_at: new Date().toISOString(),
          message: error instanceof Error ? error.message : "更新检查失败",
        };
      } finally {
        checking = null;
      }
      return state;
    })();
    return checking;
  };

  const update = async () => {
    const repoRoot = await git(toolRoot, ["rev-parse", "--show-toplevel"]);
    const dirty = await git(repoRoot, ["status", "--porcelain"]);
    if (dirty) throw new Error("工作区存在未提交修改，已拒绝自动更新");
    await git(repoRoot, ["pull", "--ff-only"]);
    await check();
    state = { ...state, status: "updated", message: "更新完成，请重新启动训练工具" };
    return state;
  };

  const respond = (res: import("node:http").ServerResponse, status: number, value: unknown) => {
    res.statusCode = status;
    res.setHeader("Content-Type", "application/json; charset=utf-8");
    res.setHeader("Cache-Control", "no-store");
    res.end(JSON.stringify(value));
  };

  return {
    name: "practice-local-system-service",
    configureServer(server) {
      const interval = Math.max(5, Number(release.update_check_interval_minutes || 30)) * 60_000;
      const initialTimer = setTimeout(() => void check(), 15_000);
      const timer = setInterval(() => void check(), interval);
      initialTimer.unref();
      timer.unref();

      server.middlewares.use(async (req, res, next) => {
        if (!req.url?.startsWith("/__practice/")) return next();
        const remote = req.socket.remoteAddress || "";
        if (!["127.0.0.1", "::1", "::ffff:127.0.0.1"].includes(remote)) {
          return respond(res, 403, { error: "仅允许本机访问" });
        }
        if (req.method === "GET" && req.url === "/__practice/system") {
          return respond(res, 200, { release, security, update: state });
        }
        if (req.method !== "POST" || req.headers["x-practice-token"] !== token) {
          return respond(res, 403, { error: "请求未通过本机会话校验" });
        }
        try {
          if (req.url === "/__practice/update/check") return respond(res, 200, await check());
          if (req.url === "/__practice/update/apply") return respond(res, 200, await update());
          return respond(res, 404, { error: "未知的固定操作" });
        } catch (error) {
          return respond(res, 409, { error: error instanceof Error ? error.message : "操作失败" });
        }
      });
    },
  };
}

function runtimeConfigPlugin(runtimeConfig: Awaited<ReturnType<typeof loadKnowledgeSources>> & { system_api_token: string }): Plugin {
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
  const toolRoot = process.cwd();
  const runtimeConfig = await loadKnowledgeSources();
  const release = readJson(path.join(toolRoot, "config", "release.json"));
  const security = readJson(path.join(toolRoot, "config", "security.json"));
  const systemApiToken = crypto.randomBytes(32).toString("hex");
  return {
    plugins: [
      react(),
      runtimeConfigPlugin({ ...runtimeConfig, system_api_token: systemApiToken }),
      systemServicePlugin(toolRoot, release, security, systemApiToken),
    ],
    base: "./",
    server: {
      host: "127.0.0.1",
    },
  };
});
