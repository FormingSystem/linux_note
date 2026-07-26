import { useEffect, useState } from "react";
import { NavLink, Outlet, useLocation, useNavigate } from "react-router-dom";
import runtimeConfig from "virtual:practice-runtime-config";

const navigation = [
  ["/", "大厅"],
  ["/library", "训练库"],
  ["/my-training", "我的训练"],
  ["/review", "复习"],
  ["/history", "历史"],
  ["/sources", "知识源"],
  ["/settings", "设置"],
] as const;

export default function AppShell() {
  const navigate = useNavigate();
  const location = useLocation();
  const inSession = location.pathname.startsWith("/sessions/");
  const [systemInfo, setSystemInfo] = useState<SystemInfo>();
  const [updateBusy, setUpdateBusy] = useState(false);
  const [updateError, setUpdateError] = useState("");
  const refreshSystemInfo = async () => {
    const response = await fetch("/__practice/system", { cache: "no-store" });
    if (response.ok) setSystemInfo(await response.json() as SystemInfo);
  };
  useEffect(() => {
    void refreshSystemInfo();
    const timer = window.setInterval(() => void refreshSystemInfo(), 60_000);
    return () => window.clearInterval(timer);
  }, []);
  const applyUpdate = async () => {
    setUpdateBusy(true);
    setUpdateError("");
    try {
      const response = await fetch("/__practice/update/apply", {
        method: "POST",
        headers: { "x-practice-token": runtimeConfig.system_api_token },
      });
      const body = await response.json() as { error?: string };
      if (!response.ok) throw new Error(body.error ?? "更新失败");
      await refreshSystemInfo();
    } catch (error) {
      setUpdateError(error instanceof Error ? error.message : "更新失败");
    } finally {
      setUpdateBusy(false);
    }
  };

  return (
    <div className="app-shell">
      <header className="topbar">
        <button className="brand brand-button" onClick={() => navigate("/")} aria-label="返回大厅">
          <span className="brand-mark">回</span>
          <span><strong>回路 · Loop</strong><small>Knowledge Practice Tool</small></span>
        </button>
        <div className="session-meta"><span className="status-dot" />本地训练 · {runtimeConfig.sources.length} 个知识源</div>
      </header>
      {systemInfo?.update.status === "available" && <div className="update-notice" role="status"><span>发现 {systemInfo.update.behind_count} 个更新 · 当前版本 {systemInfo.release.version}</span><button disabled={updateBusy} onClick={() => void applyUpdate()}>{updateBusy ? "更新中…" : "安全更新"}</button></div>}
      {systemInfo?.update.status === "updated" && <div className="update-notice" role="status"><span>{systemInfo.update.message}</span></div>}
      {updateError && <div className="update-notice update-error" role="alert"><span>更新未完成：{updateError}</span><button disabled={updateBusy} onClick={() => void applyUpdate()}>重试</button></div>}
      <nav className="app-nav" aria-label="主导航">
        {navigation.map(([path, label]) => (
          <NavLink key={path} to={path} end={path === "/"}>{label}</NavLink>
        ))}
        <div id="app-context-actions" className="app-context-actions" aria-label="当前页面操作" />
        {inSession && <button onClick={() => navigate("/")}>保存并返回大厅</button>}
      </nav>
      <main className={inSession ? "main-layout session-layout" : "main-layout"}>
        <Outlet />
      </main>
      <footer className="copyright-footer">
        <span>原创：回路（Loop）</span>
        <span>Copyright © 2026 FormingSystem · GPL-2.0-only</span>
        <a href="mailto:lizhaojun97@qq.com">联系：lizhaojun97@qq.com</a>
      </footer>
    </div>
  );
}

type SystemInfo = {
  release: { version: string };
  update: {
    status: "idle" | "checking" | "current" | "available" | "error" | "updated";
    behind_count: number;
    message: string;
  };
};
