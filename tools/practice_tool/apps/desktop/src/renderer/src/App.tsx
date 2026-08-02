import { useEffect, useState } from "react";
import type { RuntimeInfo } from "@loop/ipc-contracts";

export function App() {
  const [runtime, setRuntime] = useState<RuntimeInfo | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let active = true;
    window.loop.system.getRuntimeInfo().then(
      (value) => {
        if (active) setRuntime(value);
      },
      () => {
        if (active) setError("无法取得桌面运行时状态");
      },
    );
    return () => {
      active = false;
    };
  }, []);

  return (
    <main className="workbench">
      <header className="titlebar">
        <span className="product-mark" aria-hidden="true">◉</span>
        <span>回路（Loop）Markdown 工作台</span>
      </header>
      <section className="welcome" aria-labelledby="welcome-title">
        <p className="eyebrow">DESKTOP FOUNDATION</p>
        <h1 id="welcome-title">安全壳已经启动</h1>
        <p className="summary">
          当前切片只验证 Electron 进程隔离、窄 Preload API 与 C++ Native Service 握手。
          文件和 Markdown 能力将在后续验收项中逐项开放。
        </p>
        <dl className="runtime-card" aria-live="polite">
          <div>
            <dt>Electron</dt>
            <dd>{runtime?.electronVersion ?? "检测中"}</dd>
          </div>
          <div>
            <dt>Native Service</dt>
            <dd data-status={runtime?.nativeService.status ?? "starting"}>
              {error ?? runtime?.nativeService.message ?? "检测中"}
            </dd>
          </div>
          <div>
            <dt>协议版本</dt>
            <dd>{runtime?.nativeService.protocolVersion ?? "—"}</dd>
          </div>
        </dl>
      </section>
    </main>
  );
}
